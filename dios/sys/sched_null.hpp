// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Petr Ročkai <code@fixp.eu>
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

namespace __dios
{

    struct main_args : KObject
    {
        int l, argc;
        char **argv, **envp;
    };

    template < typename next >
    struct sched_null : public next
    {
        using task = __dios::task< typename next::Process >;
        using Task = task; /* compat */
        using process = typename next::Process;

        sched_null() :
            debug( new Debug() ),
            sighandlers( nullptr )
        {}

        ~sched_null()
        {
            delete_object( debug );
        }

        template< typename Setup >
        void setup( Setup s )
        {
            traceAlias< sched_null >( "{Scheduler}" );

            auto ma = new main_args;
            ma->l = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( main ) )->arg_count;
            std::tie( ma->argc, ma->argv ) = construct_main_arg( "arg.", s.env, true );
            ma->envp = construct_main_arg( "env.", s.env ).second;
            _task._frame = reinterpret_cast< _VM_Frame * >( ma );
            _task._proc = s.proc1;

            __vm_ctl_set( _VM_CR_Scheduler,
                          reinterpret_cast< void * >( run_scheduler< typename Setup::Context > ) );

            next::setup( s );
        }

        pid_t getpid() { return 1; }

        void exit_process( int code ) { kill_process( 0 ); }
        process *current_process() { return _task._proc; }
        task *current_task() { return &_task; }
        bool check_final() { return false; }

        void kill_task( __dios_task id )
        {
            __vm_suspend();
        }

        void kill_process( pid_t id )
        {
            __vm_suspend();
        }

        __dios_task *get_process_tasks( __dios_task tid )
        {
            process *proc;
            auto mem = __vm_obj_make( sizeof( __dios_task ), _VM_PT_Heap );
            auto ret = static_cast< __dios_task * >( mem );
            ret[0] = _task._tls;
            return ret;
        }

        int kill( pid_t pid, int sig )
        {
            return _kill( pid, sig, []( auto ){} );
        }

        template < typename F >
        int _kill( pid_t pid, int sig, F func )
        {
            sighandler_t handler;
            bool found = false;

            if ( pid != 1 && pid != -1 )
            {
                *__dios_errno() = ESRCH;
                return -1;
            }

            if ( sighandlers )
                handler = sighandlers[sig];
            else
                handler = defhandlers[sig];

            if ( handler.f == sig_ign )
                return 0;

            if ( handler.f == sig_die )
            {
                func( _task._proc );
                kill_process( pid );
            }
            else if ( handler.f == sig_fault )
                __dios_fault( _VM_F_Control, "uncaught signal" );
            else
                handler.f( sig );

            return 0;
        }

        void die() noexcept
        {
            kill_process( 0 );
        }

        __inline void prepare( task &t )
        {
            __vm_ctl_set( _VM_CR_User1, nullptr );
            __vm_ctl_set( _VM_CR_User2, t.get_id() );
            __vm_ctl_set( _VM_CR_User3, debug );
        }

        __inline void run( task &t )
        {
            prepare( t );
            auto f = reinterpret_cast< main_args * >( t._frame );
            t._frame = nullptr;
            /* switch to user mode */
            __vm_ctl_flag( _VM_CF_KernelMode | _VM_CF_IgnoreCrit | _VM_CF_IgnoreLoop, 0 );
            /* call the entry point */
            __dios_start( f->l, f->argc, f->argv, f->envp );
        }

        template < typename Context >
        static void run_scheduler() noexcept
        {
            auto& scheduler = get_state< Context >();

            task *t = &scheduler._task;
            __vm_trace( _VM_T_TaskID, t );

            if ( t->_frame )
                scheduler.run( *t ); /* does not return */

            __vm_cancel();
        }

        task _task;
        Debug *debug;
        sighandler_t *sighandlers;
    };

}
