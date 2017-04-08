// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#ifndef __DIOS_SCHEDULING_H__
#define __DIOS_SCHEDULING_H__

#include <cstring>
#include <dios.h>
#include <signal.h>
#include <sys/monitor.h>
#include <sys/signal.h>
#include <dios/core/syscall.hpp>
#include <dios/core/fault.hpp>
#include <dios/core/monitor.hpp>
#include <dios/core/syscall.hpp>
#include <divine/metadata.h>

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
    SortedStorage() : _storage( nullptr ) {}
    SortedStorage( const SortedStorage & ) = delete;
    ~SortedStorage() {
        if ( _storage )
            __vm_obj_free( _storage );
    }

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

    void resize( int n ) {
        if ( n == 0 ) {
            if ( _storage )
                __vm_obj_free( _storage );
            _storage = nullptr;
        }
        else if ( _storage )
            __vm_obj_resize( _storage, n * sizeof( T * ) );
        else
            _storage = static_cast< T **>( __vm_obj_make( n * sizeof( T * ) ) );
    }

    T **begin() noexcept { return _storage; }

    T **end() noexcept { return _storage + size(); }

    int size() const noexcept {
        return _storage ? __vm_obj_size( _storage ) / sizeof( T * ) : 0;
    }
    bool empty() const noexcept {
        return !_storage || size() == 0;
    };

    T *operator[]( int i ) const noexcept {
        return _storage[ i ];
    };
private:
    void sort() {
        if ( empty() )
            return;
        std::sort( begin(), end(), []( T *a, T *b ) {
            return a->getId() < b->getId();
        });
    }

    T **_storage;
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

    template <class F>
    Thread( void *mainFrame, void *mainTls, F routine, int tls_size ) noexcept {
        auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) );
        _frame = static_cast< _VM_Frame * >( mainFrame );
        __vm_obj_resize( _frame, fun->frame_size );
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;

        _tls = reinterpret_cast< struct _DiOS_TLS * >( mainTls );
        __vm_obj_resize( _tls, sizeof( struct _DiOS_TLS ) + tls_size );
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

void sig_ign( int );
void sig_die( int );
void sig_fault( int );

extern const sighandler_t defhandlers[ 32 ];

template < typename Next >
struct Scheduler : public Next {
    using Sys = Syscall< Scheduler< Next > >;
    Scheduler() :
        sighandlers( nullptr ),
        _global( __vm_control( _VM_CA_Get, _VM_CR_Globals ) )
    { }

    void setup( MemoryPool& pool, const _VM_Env *env, SysOpts opts ) {
        auto mainThr = newThreadMem( pool.get(), pool.get(), _start, 0 );
        auto argv = construct_main_arg( "arg.", env, true );
        auto envp = construct_main_arg( "env.", env );
        setupMainThread( mainThr, argv.first, argv.second, envp.second );

        bool notrace = extractOpt( "notrace", "thread", opts );
        bool trace = extractOpt( "trace", "thread", opts );
        if ( trace && notrace ) {
            __dios_trace_t( "Cannot trace and notrace threads" );
            __dios_fault( _DiOS_F_Config, "Thread tracing problem" );
        }

        if ( trace )
            __vm_control( _VM_CA_Set, _VM_CR_Scheduler, run_scheduler< true > );
        else
            __vm_control( _VM_CA_Set, _VM_CR_Scheduler, run_scheduler< false > );

        if ( extractOpt( "debug", "mainargs", opts ) ||
             extractOpt( "debug", "mainarg", opts ) )
        {
            trace_main_arg( 1, "main argv", argv );
            trace_main_arg( 1, "main envp", envp );
        }

        environ = envp.second;

        Next::setup( pool, env, opts );
    }

    int threadCount() const noexcept { return threads.size(); }
    Thread *chooseThread() noexcept
    {
        if ( threads.empty() )
            return nullptr;
        return threads[ __vm_choose( threads.size() ) ];
    }
    void traceThreads() const noexcept __attribute__((noinline))  {
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

    template< typename... Args >
    Thread *newThreadMem( void *mainFrame, void *mainTls, void ( *routine )( Args... ), int tls_size ) noexcept
    {
        __dios_assert_v( routine, "Invalid thread routine" );
        return threads.emplace( mainFrame, mainTls, routine, tls_size );
    }

    template< typename... Args >
    Thread *newThread( void ( *routine )( Args... ), int tls_size ) noexcept
    {
        __dios_assert_v( routine, "Invalid thread routine" );
        return threads.emplace( routine, tls_size );
    }

    void setupMainThread( Thread * t, int argc, char** argv, char** envp ) noexcept {
        DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( t->_frame );
        frame->l = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( main ) )->arg_count;

        frame->argc = argc;
        frame->argv = argv;
        frame->envp = envp;
    }

    void setupThread( Thread * t, void *arg ) noexcept {
        ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >( t->_frame );
        frame->arg = arg;
    }

    void killThread( ThreadHandle tid ) noexcept  {
        bool res = threads.remove( tid );
        __dios_assert_v( res, "Killing non-existing thread" );
    }

    void killProcess( pid_t id ) noexcept  {
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

    int sigaction( int *err, int sig, const struct ::sigaction *act, struct sigaction *oldact ) {
        if ( sig < 0 || sig > static_cast< int >( sizeof(defhandlers) / sizeof(__dios::sighandler_t) )
            || sig == SIGKILL || sig == SIGSTOP )
        {
            *err = EINVAL;
            return -1 ;
        }
        if ( !sighandlers )
        {
            sighandlers = reinterpret_cast< sighandler_t * >(
                    __vm_obj_make( sizeof( defhandlers ) ) );
            std::memcpy( sighandlers, defhandlers, sizeof(defhandlers) );
        }

        oldact->sa_handler = sighandlers[sig].f;
        sighandlers[sig].f = act->sa_handler;
        return 0;
    }

    pid_t getpid( int * ) {
        auto tid = __dios_get_thread_handle();
        auto thread = threads.find( tid );
        __dios_assert( thread );
        return thread->_pid;
    }

    _DiOS_ThreadHandle start_thread( int *, _DiOS_ThreadRoutine routine, void * arg, int tls_size ) {
        auto t = newThread( routine, tls_size );
        setupThread( t, arg );
        __vm_obj_shared( t->getId() );
        __vm_obj_shared( arg );
        return t->getId();
    }

    void kill_thread( int *, _DiOS_ThreadHandle id ) {
        killThread( id );
    }

    _DiOS_ThreadHandle *get_process_threads( int *, _DiOS_ThreadHandle tid ) {
        pid_t pid;
        for ( auto &t : threads ) {
            if ( t->_tls == tid ) {
                pid = t->_pid;
                break;
            }
        }
        int count = 0;
        for ( auto &t : threads ) {
            if ( t->_pid == pid )
                ++count;
        }
        auto ret = static_cast< _DiOS_ThreadHandle * >( __vm_obj_make( sizeof( _DiOS_ThreadHandle ) * count ) );
        int i = 0;
        for ( auto &t : threads ) {
            if ( t->_pid == pid ) {
                ret[ i ] = t->_tls;
                ++i;
            }
        }
        return ret;
    }

    int kill( int *err, pid_t pid, int sig ) {
        sighandler_t handler;
        bool found = false;
        __dios::Thread *thread;
        for ( auto t : threads )
            if ( t->_pid == pid )
            {
                found = true;
                thread = t;
                break;
            }
        if ( !found )
        {
            *err = ESRCH;
            return -1;
        }
        if ( sighandlers )
            handler = sighandlers[sig];
        else
            handler = defhandlers[sig];
        if ( handler.f == sig_ign )
            return 0;
        if ( handler.f == sig_die )
            killProcess( pid );
        else if ( handler.f == sig_fault )
            __dios_fault( _VM_F_Control, "Uncaught signal." );
        else
        {
            auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline ) );
            auto _frame = static_cast< __dios::TrampolineFrame * >( __vm_obj_make( fun->frame_size ) );
            _frame->pc = reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline );
            _frame->interrupted = thread->_frame;
            thread->_frame = _frame;
            _frame->parent = nullptr;
            _frame->handler = handler.f;
        }
        return 0;
    }

    void die( int * ) {
        killProcess( 0 );
    }

    template < bool THREAD_AWARE_SCHED >
    static void run_scheduler() noexcept
    {
        void *ctx = __vm_control( _VM_CA_Get, _VM_CR_State );
        Scheduler& scheduler = *static_cast< Scheduler * >( ctx );

        if ( THREAD_AWARE_SCHED )
            scheduler.traceThreads();
        Thread *t = scheduler.chooseThread();
        while ( t && t->_frame )
        {
            __vm_control( _VM_CA_Set, _VM_CR_User2,
                reinterpret_cast< int64_t >( t->getId() ) );
            __vm_control( _VM_CA_Set, _VM_CR_Frame, t->_frame,
                          _VM_CA_Bit, _VM_CR_Flags,
                          uintptr_t( _VM_CF_Interrupted | _VM_CF_Mask | _VM_CF_KernelMode ), 0ull );
            t->_frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_IntFrame ) );

            scheduler.runMonitors();

            auto syscall = static_cast< _DiOS_Syscall * >( __vm_control( _VM_CA_Get, _VM_CR_User1 ) );
            __vm_control( _VM_CA_Set, _VM_CR_User1, nullptr );
            if ( !syscall || Sys::handle( scheduler, *syscall ) == SchedCommand::RESCHEDULE )
                return;

            /* reset intframe to ourselves */
            auto self = __vm_control( _VM_CA_Get, _VM_CR_Frame );
            __vm_control( _VM_CA_Set, _VM_CR_IntFrame, self );
        }

        // Program ends
        scheduler.finalize();
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
    }

    SortedStorage< Thread > threads;
    sighandler_t *sighandlers;
    void *_global;
};

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
