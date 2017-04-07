// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#ifndef __DIOS_SCHEDULING_H__
#define __DIOS_SCHEDULING_H__

#include <cstring>
#include <dios.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/monitor.h>
#include <dios/core/syscall.hpp>
#include <dios/core/fault.hpp>
#include <divine/metadata.h>

namespace __sc {

void start_thread( __dios::Context& ctx, int *err, void *retval, va_list vl );
void kill_thread( __dios::Context& ctx, int *err, void *retval, va_list vl );
void kill_process( __dios::Context& ctx, int *err, void *retval, va_list vl );
void get_process_threads( __dios::Context &ctx, int *err, void *_ret, va_list vl );


} // namespace __sc

namespace __dios {

using ThreadHandle = _DiOS_ThreadHandle;

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

struct TrampolineFrame : _VM_Frame {
    _VM_Frame * interrupted;
    void ( *handler )( int );
};

template < class T >
struct SortedStorage {
    using Tid = decltype( std::declval< T >().getId() );
    SortedStorage() {}
    SortedStorage( const SortedStorage & ) = delete;

    T *find( Tid id ) noexcept {
        for ( auto t : *this )
            if ( t->getId() == id )
                return t;
        return nullptr;
    }

    bool remove( Tid id ) noexcept {
        int s = size();
        for ( int i = 0; i != s; i++ ) {

            if ( _storage[ i ]->getId() != id )
                continue;
            delete_object( _storage[ i ] );
            _storage[ i ] = _storage[ s - 1 ];
            resize ( size() - 1 );
            sort();
            return true;
        }
        return false;
    }

    template < class... Args >
    T *emplace( Args... args ) noexcept {
        resize( size() + 1 );
        int idx = size() - 1;
        T *ret = _storage[ idx ] = new_object< T >( args... );
        sort();
        return ret;
    }

    void erase( T** first, T** last ) noexcept {
        if ( empty() )
            return;
        int orig_size = size();
        for ( T** f = first; f != last; f++ ) {
            delete_object( *f );
        }
        int s = last - first;
        if ( s != orig_size )
            memmove( first, last, ( end() - last ) * sizeof( T * ) );
        resize( orig_size - s );
        sort();
    }

    void resize( int n ) { __vm_obj_resize( this, std::max( 1,
                                    static_cast< int > (sizeof( *this ) + n * sizeof( T * ) ) ) ); }
    T **begin() noexcept { return _storage; }
    T **end() noexcept { return _storage + size(); }
    int size() const noexcept { return ( __vm_obj_size( this ) - sizeof( *this ) ) / sizeof( T * ); }
    bool empty() const noexcept { return size() == 0; };
    T *operator[]( int i ) const noexcept { return _storage[ i ]; };
private:
    void sort() {
        if ( empty() )
            return;
        std::sort( begin(), end(), []( T *a, T *b ) {
            return a->getId() < b->getId();
        });
    }

    T *_storage[];
};

struct Thread {
    _VM_Frame *_frame;
    struct _DiOS_TLS *_tls;
    pid_t _pid;

    template <class F>
    Thread( F routine, int tls_size ) noexcept {
        auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) );
        _frame = static_cast< _VM_Frame * >( __vm_obj_make( fun->frame_size ) );
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;

        _tls = reinterpret_cast< struct _DiOS_TLS * >
            ( __vm_obj_make( sizeof( struct _DiOS_TLS ) + tls_size ) );
        _tls->_errno = 0;
        _pid = 1; // ToDo: Add process support
    }

    Thread( const Thread& o ) noexcept = delete;
    Thread& operator=( const Thread& o ) noexcept = delete;

    Thread( Thread&& o ) noexcept;
    Thread& operator=( Thread&& o ) noexcept;

    ~Thread() noexcept;

    bool active() const noexcept { return _frame; }
    ThreadHandle getId() const noexcept { return _tls; }
    uint32_t getUserId() const noexcept {
        auto tid = reinterpret_cast< uint64_t >( _tls ) >> 32;
        return static_cast< uint32_t >( tid );
    }

private:
    void clearFrame() noexcept;
};

static void sig_ign( int ) {}
static void sig_die( int ) {}
static void sig_fault( int ) {}

extern const sighandler_t defhandlers[ 32 ];

struct Scheduler {
    Scheduler() {}
    int threadCount() const noexcept { return threads.size(); }

    template< typename... Args >
    Thread *newThread( void ( *routine )( Args... ), int tls_size ) noexcept
    {
        __dios_assert_v( routine, "Invalid thread routine" );
        return threads.emplace( routine, tls_size );
    }

    Thread *chooseThread() noexcept
    {
        if ( threads.empty() )
            return nullptr;
        return threads[ __vm_choose( threads.size() ) ];
    }

    void traceThreads() const noexcept __attribute__((noinline))
    {
        int c = threads.size();
        if ( c == 0 )
            return;
        struct PI { unsigned pid, tid, choice; };
        PI *pi = reinterpret_cast< PI * >( __vm_obj_make( c * sizeof( PI ) ) );
        PI *pi_it = pi;
        for ( int i = 0; i != c; i++ ) {
            pi_it->pid = 0;
            pi_it->tid = threads[ i ]->getUserId();
            pi_it->choice = i;
            ++pi_it;
        }

        __vm_trace( _VM_T_SchedChoice, pi );
        __vm_obj_free( pi );
    }

    void setupMainThread( Thread *t, int argc, char** argv, char** envp ) noexcept
    {
        DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( t->_frame );
        frame->l = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( main ) )->arg_count;

        frame->argc = argc;
        frame->argv = argv;
        frame->envp = envp;
    }

    void setupThread( Thread *t, void *arg ) noexcept
    {
        ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >( t->_frame );
        frame->arg = arg;
    }

    void killThread( ThreadHandle tid ) noexcept
    {
        bool res = threads.remove( tid );
        __dios_assert_v( res, "Killing non-existing thread" );
    }

    void killProcess( pid_t id ) noexcept
    {
        if ( !id ) {
            threads.erase( threads.begin(), threads.end() );
            // ToDo: Erase processes
            return;
        }

        auto r = std::remove_if( threads.begin(), threads.end(), [&]( Thread *t ) {
                return t->_pid == id;
            });
        threads.erase( r, threads.end() );
        // ToDo: Erase processes
    }

    static void sigaction( __dios::Context& ctx, int *err, void *ret, va_list vl )
    {
        int sig = va_arg( vl, int );
        auto act = va_arg( vl, const struct sigaction * );
        auto oldact = va_arg( vl, struct sigaction * );

        if ( sig < 0 ||
             sig > static_cast<int>( sizeof(defhandlers) / sizeof( sighandler_t ) ) ||
             sig == SIGKILL || sig == SIGSTOP )
        {
            *static_cast< int * >( ret ) = -1;
            *err = EINVAL;
            return;
        }

        if ( !ctx.sighandlers )
        {
            ctx.sighandlers = reinterpret_cast< sighandler_t * >(
                __vm_obj_make( sizeof( defhandlers ) ) );
            std::memcpy( ctx.sighandlers, defhandlers, sizeof( defhandlers ) );
        }

        oldact->sa_handler = ctx.sighandlers[sig].f;
        ctx.sighandlers[sig].f = act->sa_handler;
        *static_cast< int * >( ret ) = 0;
    }

    static void getpid( __dios::Context& ctx, int *, void *retval, va_list )
    {
        auto tid = __dios_get_thread_handle();
        auto thread = ctx.scheduler->threads.find( tid );
        __dios_assert( thread );
        *static_cast< int * >( retval ) = thread->_pid;
    }

    static void start_thread( __dios::Context& ctx, int *, void *retval, va_list vl )
    {
        typedef void ( *r_type )( void * );
        auto routine = va_arg( vl, r_type );
        auto arg = va_arg( vl, void * );
        auto tls = va_arg( vl, int );
        auto ret = static_cast< __dios::ThreadHandle * >( retval );

        auto t = ctx.scheduler->newThread( routine, tls );
        ctx.scheduler->setupThread( t, arg );
        __vm_obj_shared( t->getId() );
        __vm_obj_shared( arg );
        *ret = t->getId();
    }

    static void kill_thread( __dios::Context& ctx, int *, void *, va_list vl )
    {
        auto id = va_arg( vl, __dios::ThreadHandle );
        ctx.scheduler->killThread( id );
    }

    static void kill_process( __dios::Context& ctx, int *, void *, va_list vl )
    {
        auto id = va_arg( vl, pid_t );
        ctx.scheduler->killProcess( id );
    }

    static void get_process_threads( __dios::Context &ctx, int *, void *_ret, va_list vl )
    {
        auto *&ret = *reinterpret_cast< _DiOS_ThreadHandle ** >( _ret );
        auto tid = va_arg( vl, _DiOS_ThreadHandle );
        pid_t pid;
        for ( auto &t : ctx.scheduler->threads ) {
            if ( t->_tls == tid ) {
                pid = t->_pid;
                break;
            }
        }
        int count = 0;
        for ( auto &t : ctx.scheduler->threads ) {
            if ( t->_pid == pid )
                ++count;
        }
        ret = static_cast< _DiOS_ThreadHandle * >(
            __vm_obj_make( sizeof( _DiOS_ThreadHandle ) * count ) );
        int i = 0;
        for ( auto &t : ctx.scheduler->threads ) {
            if ( t->_pid == pid ) {
                ret[ i ] = t->_tls;
                ++i;
            }
        }
    }

    static void kill( __dios::Context& ctx, int *err, void *ret, va_list vl )
    {
        int pid = va_arg( vl, pid_t );
        auto sig = va_arg( vl, int );
        sighandler_t handler;

        bool found = false;
        __dios::Thread *thread;
        for ( auto t : ctx.scheduler->threads )
            if ( t->_pid == pid )
            {
                found = true;
                thread = t;
                break;
            }

        if ( !found )
        {
            *err = ESRCH;
            *static_cast< int* >( ret ) = -1;
            return;
        }

        if ( ctx.sighandlers )
            handler = ctx.sighandlers[sig];
        else
            handler = defhandlers[sig];

        *static_cast< int* >( ret ) = 0;

        if ( handler.f == sig_ign )
            return;
        if ( handler.f == sig_die )
            ctx.scheduler->killProcess( pid );
        else if ( handler.f == sig_fault )
            __dios_fault( _VM_F_Control, "Uncaught signal." );
        else
        {
            auto fun = __md_get_pc_meta(
                reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline ) );
            auto _frame = static_cast< __dios::TrampolineFrame * >( __vm_obj_make( fun->frame_size ) );
            _frame->pc = reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline );
            _frame->interrupted = thread->_frame;
            thread->_frame = _frame;
            _frame->parent = nullptr;
            _frame->handler = handler.f;
        }
    }

    SortedStorage< Thread > threads;
};

template < bool THREAD_AWARE_SCHED >
void sched() noexcept
{
    auto ctx = static_cast< Context * >( __vm_control( _VM_CA_Get, _VM_CR_State ) );
    if ( THREAD_AWARE_SCHED )
        ctx->scheduler->traceThreads();
    Thread *t = ctx->scheduler->chooseThread();
    while ( t && t->_frame )
    {
        __vm_control( _VM_CA_Set, _VM_CR_User2,
            reinterpret_cast< int64_t >( t->getId() ) );
        __vm_control( _VM_CA_Set, _VM_CR_Frame, t->_frame,
                      _VM_CA_Bit, _VM_CR_Flags,
                      uintptr_t( _VM_CF_Interrupted | _VM_CF_Mask | _VM_CF_KernelMode ), 0ull );
        t->_frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_IntFrame ) );

        if ( ctx->monitors )
            ctx->monitors->run( *ctx );

        Syscall *syscall = static_cast< Syscall * >( __vm_control( _VM_CA_Get, _VM_CR_User1 ) );
        __vm_control( _VM_CA_Set, _VM_CR_User1, nullptr );
        if ( !syscall || syscall->handle( ctx ) == SchedCommand::RESCHEDULE )
            return;

        /* reset intframe to ourselves */
        auto self = __vm_control( _VM_CA_Get, _VM_CR_Frame );
        __vm_control( _VM_CA_Set, _VM_CR_IntFrame, self );

    }

    // Program ends
    ctx->finalize();
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
}

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
