// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Jan Mrázek <email@honzamrazek.cz>
 *     2016 Vladimir Still <xstill@fi.muni.cz>
 *     2016, 2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <algorithm>
#include <unistd.h>

#include <sys/utsname.h>
#include <sys/bitcode.h>
#include <sys/metadata.h>

#include <dios.h>
#include <dios/sys/options.hpp>

#include <dios/vfs/manager.h>

/* This file implements the boot sequence of DiOS. The real entrypoint is
 * extern "C" __boot(), implemented in config. It doesn't do anything other
 * than call __dios::boot with the correct template parameter. Most of this
 * file is concerned with parsing the environment passed down by the platform. */

extern "C" void *__dios_term_init();

namespace __dios
{
    void trace_option( int i, std::string_view opt, std::string_view desc,
                       const Array< std::string_view >& args )
    {
        __dios_trace_i( i, "- %.*s: %.*s",
                        int( opt.size() ), opt.begin(), int( desc.size() ), desc.begin() );
        __dios_trace_i( i, "  arguments:" );
        for ( const auto& arg : args )
            __dios_trace_i( i, "   - %.*s", int( arg.size() ), arg.begin() );
    }

    void trace_help( int i, const ArrayMap< std::string_view, HelpOption >& help )
    {
        for ( const auto& option : help )
            trace_option( i, option.first, option.second.description,
                          option.second.options );
    }

    void trace_env( int ind, const _VM_Env *env )
    {
        __dios_trace_i( ind, "raw env options:" );
        for ( ; env->key; env++ ) {
            __dios_trace_i( ind + 1, "%s: \"%.*s\"", env->key, env->size, env->value );
        }
    }

    void temporary_scheduler()
    {
        __vm_cancel();
    }

    void temporary_fault( _VM_Fault, _VM_Frame *, void (*)() )
    {
        __vm_ctl_flag( 0, _VM_CF_Error );
        __vm_ctl_set( _VM_CR_Scheduler, nullptr );
        __dios_this_frame()->parent = nullptr;
    }

    /* The configuration-specific (but platform-neutral) entry point.
     * Instantiated in `dios/config/?.cpp`. Handles environment parsing and
     * basic platform setup and then creates an instance of the configuration
     * stack. After boot() is done, the platform is expected to call the
     * scheduler. This is either done by a platform-specific entry point (e.g.
     * `klee_boot`) or by the platform itself (e.g. DiVM). */

    template < typename Configuration >
    void boot( const _VM_Env *env )
    {
        /* On DiVM, addresses of objects obtained from __vm_obj_make depend on
         * the execution history until that point. If two executions differ in
         * just a single system call, the object numbering will be different
         * and things that depend on it will change behaviour. This used to
         * cause problems with counterexample replays. This causes memory for
         * persistent objects to be allocated before any decisions based on
         * configuration options are made. I also think this is no longer
         * needed on modern DiVM with dbg.call and with counterexamples
         * embedded in verification reports. */

        MemoryPool deterministic_memory( 2 );
        SysOpts opt;

        __vm_ctl_set( _VM_CR_User1, nullptr );
        __vm_ctl_set( _VM_CR_FaultHandler, reinterpret_cast< void * >( temporary_fault ) );
        __vm_ctl_set( _VM_CR_Scheduler, reinterpret_cast< void * >( temporary_scheduler ) );

        if ( !parse_sys_options( env, opt ) )
        {
            __vm_ctl_flag( 0, _VM_CF_Error );
            return;
        }

        auto *context = new Configuration();
        __vm_trace( _VM_T_StateType, context );
        traceAlias< Configuration >( "{Context}" );
        __vm_ctl_set( _VM_CR_State, context );

        if ( extract_opt( "debug", "help", opt ) ) /* please send hulp */
        {
            ArrayMap< std::string_view, HelpOption > help;

            help.emplace( "debug", HelpOption
                          { "print debug information during boot",
                             { "help - help of selected configuration and exit",
                               // ToDo: trace binary blobs
                               /*"rawenvironment - user DiOS boot parameters",*/
                               "machineparams - specified by user, e.g. number of cpus",
                               "mainargs - argv and envp",
                               "faultcfg - fault and simfail configuration" } } );

            context->getHelp( help );
            trace_help( 0, help );
            __vm_cancel();
            return;
        }

        if ( extract_opt( "debug", "rawenvironment", opt ) )
            trace_env( 1, env );

        /* The Setup structure is passed down the stack and each component in
         * turn parses the options that it understands and removes them from
         * .opts. If some options remain when we hit bottom, that's an error.
         * The structure is passed by value, hence the removal of options does
         * not propagate back up. */
        Setup< Configuration > s = SetupBase{ .pool = &deterministic_memory,
                                              .env = env, .opts = opt };

        __vm_trace( _VM_T_Constraints, __dios_term_init() );

        /* The first process is special and is created during boot. Other
         * processes can only be created using `fork`. Components that maintain
         * per-process data should initialize their fields in proc1. The
         * Process structure itself is stacked the same way the configuration
         * stack is. */
        s.proc1 = new typename Configuration::Process();
        context->setup( s );
    }
}
