// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#ifndef __DIOS_SCHEDULING_H__
#define __DIOS_SCHEDULING_H__

#include <cstring>
#include <divine/dios.h>
#include <divine/dios/syscall.h>

// Syscall implementation prototypes
void __sc_start_thread( void *retval, va_list vl );
void __sc_kill_thread( void *retval, va_list vl );
void __sc_get_thread_id( void *retval, va_list vl );

namespace __dios {

using ThreadId = _DiOS_ThreadId;
using FunPtr   = _DiOS_FunPtr;

struct DiosMainFrame : _VM_Frame {
    int l;
    int argc;
    char** argv;
    char** envp;
};

struct ThreadRoutineFrame : _VM_Frame {
    void* arg;
};

struct CleanupFrame : _VM_Frame {
    int reason;
};

struct Thread {
    enum class State { RUNNING, CLEANING_UP, ZOMBIE };
    _VM_Frame *_frame;
    FunPtr _cleanup_handler;
    State _state;

    Thread( FunPtr fun, FunPtr cleanup = nullptr )
        : _frame( static_cast< _VM_Frame * >( __vm_make_object( fun->frame_size ) ) ),
          _cleanup_handler( cleanup ),
          _state( State::RUNNING )
    {
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;
        __dios_trace_f( "Thread constuctor: %p, %p", _frame, _frame->pc );
    }

    Thread( const Thread& o ) = delete;
    Thread& operator=( const Thread& o ) = delete;

    Thread( Thread&& o ) :
        _frame( o._frame ), _cleanup_handler( o._cleanup_handler ),
        _state( o._state )
    {
        o._frame = 0;
        o._state = State::ZOMBIE;
    }

    Thread& operator=( Thread&& o ) {
        std::swap( _frame, o._frame );
        std::swap( _cleanup_handler, o._cleanup_handler );
        std::swap( _state, o._state );
        return *this;
    }

    ~Thread() {
        clear();
    }

    bool active() const { return _state == State::RUNNING; }
    bool cleaning_up() const { return _state == State::CLEANING_UP; }
    bool zombie() const { return _state == State::ZOMBIE; }

    void update_state() {
        if ( !_frame )
            _state = State::ZOMBIE;
    }

    void stop_thread( int reason ) {
        if ( !active() ) {
            __vm_fault( static_cast< _VM_Fault >( _DiOS_F_Threading ) );
            return;
        }

        clear();
        auto* frame = reinterpret_cast< CleanupFrame * >( _frame );
        frame->pc = _cleanup_handler->entry_point;
        frame->parent = nullptr;
        frame->reason = reason;
        _state = State::CLEANING_UP;
    }

private:
    void clear() {
        while ( _frame ) {
            _VM_Frame *f = _frame->parent;
            __vm_free_object( _frame );
            _frame = f;
        }
    }
};

struct ControlFlow {
    ThreadId active_thread;
    int      thread_count;
    Thread   main_thread;
};

struct Scheduler {
    Scheduler() {
        __dios_assert( _cf );
    }

    Scheduler( void *cf ) {
        _cf =  static_cast< ControlFlow * >( cf );
        __dios_assert( cf );
    }

    Thread* get_threads() const noexcept {
        return &( _cf->main_thread );
    }

    void *get_cf() const noexcept {
        return _cf;
    }

    _VM_Frame *run_live_thread() {
        int count = 0;
        for ( int i = 0; i != _cf->thread_count; i++ ) {
            if ( get_threads()[ i ].active() )
                count++;
        }

        if ( count == 0 )
            return nullptr;

        struct PI { int pid, tid, choice; };
        PI *pi = reinterpret_cast< PI * >( __vm_make_object( count * sizeof( PI ) ) );
        PI *pi_it = pi;
        count = 0;
        for ( int i = 0; i != _cf->thread_count; i++ ) {
            if ( get_threads()[ i ].active() ) {
                pi_it->pid = 1;
                pi_it->tid = i;
                pi_it->choice = count++;
                ++pi_it;
            }
        }

        __vm_trace( _VM_T_SchedChoice, pi );
        int sel = __vm_choose( count );
        auto thr = get_threads();
        while ( sel != 0 ) {
            if ( thr->active() )
                --sel;
            ++thr;
        }

        _cf->active_thread = thr - get_threads();
        __vm_set_ifl( &( thr->_frame) );
        __dios_assert_v( thr->_frame, "Frame is invalid" );
        return thr->_frame;
    }

    template < bool THREAD_AWARE >
    _VM_Frame *run_thread( int idx = -1 ) noexcept {
        get_threads()[ _cf->active_thread ].update_state();
        if ( !THREAD_AWARE )
        {
            if ( idx < 0 )
                idx = _cf->active_thread;
            else
                _cf->active_thread = idx;

            Thread &thread = get_threads()[ idx ];
            if ( !thread.zombie() ) {
                __vm_set_ifl( &( thread._frame) );
                return thread._frame;
            }

            __dios_trace_t( "Thread exit" );
        }

        if ( THREAD_AWARE )
            return run_live_thread();

        _cf = nullptr;
        return nullptr;
    }

    template < bool THREAD_AWARE >
    _VM_Frame *run_threads() noexcept {
        // __dios_trace( 0, "Number of threads: %d", _cf->thread_count );
        return run_thread< THREAD_AWARE >( THREAD_AWARE ? 0 : __vm_choose( _cf->thread_count ) );
    }

    void start_main_thread( FunPtr main, int argc, char** argv, char** envp ) noexcept {
        __dios_assert_v( main, "Invalid main function!" );

        _DiOS_FunPtr dios_main = __dios_get_fun_ptr( "__dios_main" );
        __dios_assert_v( dios_main, "Invalid DiOS main function" );

        new ( &( _cf->main_thread ) ) Thread( dios_main );
        _cf->active_thread = 0;
        _cf->thread_count = 1;

        DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( _cf->main_thread._frame );
        frame->l = main->arg_count;

        frame->argc = argc;
        frame->argv = argv;
        frame->envp = envp;
    }

    ThreadId start_thread( FunPtr routine, void *arg, FunPtr cleanup ) {
        __dios_assert( routine );

        int cur_size = __vm_query_object_size( _cf );
        void *new_cf = __vm_make_object( cur_size + sizeof( Thread ) );
        memcpy( new_cf, _cf, cur_size );
        __vm_free_object( _cf );
        _cf = static_cast< ControlFlow * >( new_cf );

        Thread &t = get_threads()[ _cf->thread_count++ ];
        new ( &t ) Thread( routine, cleanup );
        ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >(
            t._frame );
        frame->arg = arg;

        return _cf->thread_count - 1;
    }

    void kill_thread( ThreadId t_id, int reason ) {
        __dios_assert( t_id );
        __dios_assert( int( t_id ) < _cf->thread_count );
        get_threads()[ t_id ].stop_thread( reason );
    }

    ThreadId get_thread_id() {
        return _cf->active_thread;
    }
private:
    static ControlFlow* _cf;
};

template < bool THREAD_AWARE_SCHED >
void *sched( int, void *state ) noexcept {
    Scheduler scheduler( state );
    if ( Syscall::get().handle() ) {
        __vm_jump( scheduler.run_thread< THREAD_AWARE_SCHED >(), nullptr, 1 );
        return scheduler.get_cf();
    }

    _VM_Frame *jmp = scheduler.run_threads< THREAD_AWARE_SCHED >();
    if ( jmp ) {
        __vm_jump( jmp, nullptr, 1 );
        return scheduler.get_cf();
    }

    return nullptr;
}

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
