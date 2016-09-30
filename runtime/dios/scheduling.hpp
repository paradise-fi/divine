// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#ifndef __DIOS_SCHEDULING_H__
#define __DIOS_SCHEDULING_H__

#include <cstring>
#include <dios.h>
#include <dios/syscall.hpp>
#include <dios/fault.hpp>

namespace __sc {

void start_thread( __dios::Context& ctx, void *retval, va_list vl );
void kill_thread( __dios::Context& ctx, void *retval, va_list vl );
void get_thread_id( __dios::Context& ctx, void *retval, va_list vl );

} // namespace __sc

namespace __dios {

using ThreadId = _DiOS_ThreadId;

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
    enum class State { RUNNING, ZOMBIE };
    _VM_Frame *_frame;
    State _state;

    Thread( void ( *fun )( void * ) ) noexcept;
    Thread( void ( *main )( int, int, char **, char ** ) ) noexcept;
    Thread( const Thread& o ) noexcept = delete;
    Thread& operator=( const Thread& o ) noexcept = delete;
    Thread( Thread&& o ) noexcept;
    Thread& operator=( Thread&& o ) noexcept;
    ~Thread() noexcept { clear(); }

    bool active() const noexcept { return _state == State::RUNNING; }
    bool zombie() const noexcept { return _state == State::ZOMBIE; }

    void update_state() noexcept;
    void stop() noexcept;
    void hard_stop() noexcept;

private:
    void clear() noexcept;
};

struct ControlFlow {
    ThreadId active_thread;
    int      thread_count;
    Thread   main_thread[ 0 ];
};

struct Scheduler {
    Scheduler() :
        _cf( static_cast< ControlFlow * >(
            __vm_obj_make( sizeof( ControlFlow ) + sizeof( Thread ) ) ) )
    {
        _cf->active_thread = -1;
        _cf->thread_count = 0;
    }

    Thread *get_threads() const noexcept { return &( _cf->main_thread[ 0 ] ); }
    Thread *get_active_thread() noexcept { return &get_threads()[ _cf->active_thread ]; }
    Thread *choose_thread() noexcept;
    Thread *choose_live_thread() noexcept;

    void start_main_thread( int ( *main )( ... ), int argc, char** argv, char** envp ) noexcept;
    ThreadId start_thread( void ( *routine )( void * ), void *arg ) noexcept;

    void kill_thread( ThreadId t_id ) noexcept;
    void terminate() noexcept;

    ThreadId get_thread_id() {
        return _cf->active_thread;
    }
private:
    ControlFlow* _cf;
};

template < bool THREAD_AWARE_SCHED >
void sched() noexcept
{
    auto ctx = static_cast< Context * >( __vm_control( _VM_CA_Get, _VM_CR_State ) );

    if ( ctx->fault->triggered )
    {
        ctx->scheduler->terminate();
        ctx->fault->triggered = false;
        return;
    }

    Thread *t = THREAD_AWARE_SCHED ?
                ctx->scheduler->choose_live_thread() :
                ctx->scheduler->choose_thread();

    while ( t && t->_frame )
    {
        __vm_control( _VM_CA_Set, _VM_CR_Frame, t->_frame,
                      _VM_CA_Bit, _VM_CR_Flags,
                      uintptr_t( _VM_CF_Interrupted | _VM_CF_Mask | _VM_CF_KernelMode ), 0ull );
        t->_frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_IntFrame ) );

        if ( !ctx->syscall->handle( ctx ) )
            return;

        /* reset intframe to ourselves */
        auto self = __vm_control( _VM_CA_Get, _VM_CR_Frame );
        __vm_control( _VM_CA_Set, _VM_CR_IntFrame, self );
    }

    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
}

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
