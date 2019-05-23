// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Jan Mrázek <email@honzamrazek.cz>
 *     2016 Vladimir Still <xstill@fi.muni.cz>
 *     2016, 2018 Petr Ročkai <code@fixp.eu>
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

#pragma once

#include <algorithm>
#include <cstdarg>
#include <sys/metadata.h>
#include <sys/trace.h>

#include <dios/sys/syscall.hpp>

/* There are two things going on in this file. The first is configuration of
 * simulated failures from `malloc`, via the `{simfail,nofail}:malloc` options.
 * It doesn't really belong here and should be moved (TODO). */

/* The second is the actual fault handler, which serves two purposes: it
 * decides whether a particular fault is ‘interesting’, and if it is, it asks
 * the VM to report a counterexample to the user. There are two ways to enter
 * the fault handler: either the VM pushes it onto the stack because it
 * detected an illegal operation (e.g. out of bounds access), or library code
 * can enter it via `__dios_fault` (e.g. when an assertion fails). */

namespace __dios
{

    enum FaultFlag
    {
        Enabled       = 0x01,
        Continue      = 0x02,
        UserSpec      = 0x04,
        AllowOverride = 0x08,
        Artificial    = 0x10,
        Detect        = 0x20
    };

    struct ProcessFaultInfo
    {
        ProcessFaultInfo()
        {
            std::fill_n( faultConfig, _DiOS_SF_Last, FaultFlag::Enabled | FaultFlag::AllowOverride );
        }

        uint8_t faultConfig[ _DiOS_SF_Last ];
    };

    /* This class is intentionally not part of the stack, because nothing
     * inside uses services of other components. This means the code for the
     * methods is only generated once instead of once per configuration, which
     * reduces the bitcode size a bit. */

    struct FaultBase
    {
        virtual uint8_t *config() = 0;
        virtual void die() = 0;

        uint8_t _flags;
        enum Flags { Ready = 1, PrintBacktrace = 2 };
        static constexpr int fault_count = _DiOS_SF_Last;

        struct MallocNofailGuard {
        private:
            MallocNofailGuard( FaultBase & f )
                : _f( f ),
                  _config( _f.config() ),
                  _s( _config[ _DiOS_SF_Malloc ] )
            {
                _config[ _DiOS_SF_Malloc ] = 0;
                _f.updateSimFail();
            }
        public:
            ~MallocNofailGuard() {
                _config[ _DiOS_SF_Malloc ] = _s;
                _f.updateSimFail();
            }

            FaultBase& _f;
            uint8_t *_config;
            uint8_t _s;

            friend FaultBase;
        };

        MallocNofailGuard makeMallocNofail()
        {
            return { *this };
        }

        FaultBase() : _flags( 0 ) {}

        /* This is the actual entry point of the fault handler, the address of
         * which is stored in _VM_CR_FaultHandler. It's also a __trapfn because
         * we need to switch to kernel mode if we got called from
         * `__dios_fault`. The actual logic is implemented in `fault_handler()`
         * below. */

        template < typename Context >
        static __trapfn void handler( _VM_Fault _what, _VM_Frame *cont_frame,
                                      void (*cont_pc)() ) noexcept
        {
            uint64_t old = __vm_ctl_flag( 0, _VM_CF_KernelMode | _VM_CF_IgnoreCrit |
                                             _VM_CF_IgnoreLoop | _DiOS_CF_IgnoreFault );

            if ( old & _DiOS_CF_IgnoreFault )
                goto ret;

            __dios_sync_parent_frame();

            {
                bool kernel = old & _VM_CF_KernelMode;
                auto *frame = __dios_parent_frame();
                auto& fault = get_state< Context >();
                fault.fault_handler( kernel, frame, _what );
            }

            // Continue if we get the control back
            old |= uint64_t( __vm_ctl_get( _VM_CR_Flags ) ) & ( _DiOS_CF_Fault | _VM_CF_Error );
            // clean possible intermediate frames to avoid memory leaks
            __dios_stack_free( __dios_parent_frame(), cont_frame );
          ret:
            __vm_ctl_set( _VM_CR_Flags, reinterpret_cast< void * >( old ) );
            __vm_ctl_set( _VM_CR_Frame, cont_frame, cont_pc );
            __builtin_trap();
        }

        void updateSimFail( uint8_t *config = nullptr );
        void backtrace( _VM_Frame * frame );
        void fault_handler( int kernel, _VM_Frame * frame, int what );

        static int str_to_fault( std::string_view fault );
        static std::string_view fault_to_str( int f, bool ext = false );
        void trace_config( int indent );
        void load_user_pref( uint8_t *config, SysOpts& opts );
        int _faultToStatus( int fault );
        int configure_fault( int fault, int cfg );
        int get_fault_config( int fault );
    };

    template < typename Next >
    struct Fault: FaultBase, Next
    {

        struct Process : Next::Process, ProcessFaultInfo {};

        template< typename Setup >
        void setup( Setup s )
        {
            traceAlias< Fault >( "{Fault}" );
            __vm_ctl_set( _VM_CR_FaultHandler,
                          reinterpret_cast< void * >( handler< typename Setup::Context > ) );
            load_user_pref( s.proc1->faultConfig, s.opts );

            if ( extract_opt( "debug", "faultcfg", s.opts ) )
            {
                trace_config( 1 );
                __vm_ctl_flag( 0, _VM_CF_Error );
            }
            _flags = Ready;
            if ( extract_opt( "debug", "faultbt", s.opts ) )
                _flags |= PrintBacktrace;
            Next::setup( s );
        }

        void getHelp( ArrayMap< std::string_view, HelpOption >& options )
        {
            const char *opt1 = "[force-]{ignore|report|abort}";
            if ( options.find( opt1 ) != options.end() ) {
                __dios_trace_f( "Option %s already present", opt1 );
                __dios_fault( _DiOS_F_Config, "Option conflict" );
            }
            options[ opt1 ] = { "configure the fault handler",
                                { "assert", "arithmetic", "memory", "control",
                                  "locking", "hypercall", "notimplemented", "diosassert" } };

            const char *opt2 = "{nofail|simfail}";
            if ( options.find( opt2 ) != options.end() ) {
                __dios_trace_f( "Option %s already present", opt2 );
                __dios_fault( _DiOS_F_Config, "Option conflict" );
            }
            options[ opt2 ] = { "enable/disable simulation of failure",
              { "malloc" } };

            Next::getHelp( options );
        }

        uint8_t *config() override
        {
            auto *task = Next::current_task();
            if ( !task )
                return nullptr;
            return static_cast< Process * >( task->_proc )->faultConfig;
        }

        void die() override { Next::die(); }

        using FaultBase::get_fault_config;
        using FaultBase::configure_fault;
        using FaultBase::fault_handler;
    };

}
