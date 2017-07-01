// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#ifndef __DIOS_SCHEDULING_H__
#define __DIOS_SCHEDULING_H__

#include <cstring>
#include <signal.h>
#include <sys/monitor.h>
#include <sys/signal.h>
#include <sys/start.h>
#include <dios.h>

#include <dios/core/main.hpp>
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

    void insert( T* t ) noexcept {
        resize( size() + 1 );
        int idx = size() - 1;
        _storage[ idx ] = t;
        sort();
    }

    void erase( T** first, T** last ) noexcept {
        if ( empty() )
            return;
        int orig_size = size();
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

template < typename Process >
struct Thread {
    _VM_Frame *_frame;
    struct _DiOS_TLS *_tls;
    Process *_proc;

    template <class F>
    Thread( F routine, int tls_size, Process *proc ) noexcept
        : _proc( proc )
    {
        auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) );
        _frame = static_cast< _VM_Frame * >( __vm_obj_make( fun->frame_size ) );
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;

        _tls = reinterpret_cast< struct _DiOS_TLS * >
            ( __vm_obj_make( sizeof( struct _DiOS_TLS ) + tls_size ) );
        _tls->_errno = 0;
    }

    template <class F>
    Thread( void *mainFrame, void *mainTls, F routine, int tls_size, Process *proc ) noexcept
        : _proc( proc )
    {
        auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) );
        _frame = static_cast< _VM_Frame * >( mainFrame );
        __vm_obj_resize( _frame, fun->frame_size );
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;

        _tls = reinterpret_cast< struct _DiOS_TLS * >( mainTls );
        __vm_obj_resize( _tls, sizeof( struct _DiOS_TLS ) + tls_size );
        _tls->_errno = 0;
    }

    Thread( const Thread& o ) noexcept = delete;
    Thread& operator=( const Thread& o ) noexcept = delete;

    Thread( Thread&& o ) noexcept
        : _frame( o._frame ), _tls( o._tls ), _proc( o._proc )
    {
        o._frame = nullptr;
        o._tls = nullptr;
    }

    Thread& operator=( Thread&& o ) noexcept {
        std::swap( _frame, o._frame );
        std::swap( _tls, o._tls );
        std::swap( _proc, o._proc );
        return *this;
    }

    ~Thread() noexcept {
        clearFrame();
        __vm_obj_free( _tls );
    }

    bool active() const noexcept { return _frame; }
    ThreadHandle getId() const noexcept { return _tls; }
    uint32_t getUserId() const noexcept {
        auto tid = reinterpret_cast< uint64_t >( _tls ) >> 32;
        return static_cast< uint32_t >( tid );
    }

private:

    void clearFrame() noexcept {
        while ( _frame ) {
            _VM_Frame *f = _frame->parent;
            __vm_obj_free( _frame );
            _frame = f;
        }
    }
};

#ifdef __dios_kernel__

void sig_ign( int );
void sig_die( int );
void sig_fault( int );

extern const sighandler_t defhandlers[ 32 ];

template < typename Next >
struct Scheduler : public Next {
    using Sys = Syscall< Scheduler< Next > >;
    Scheduler() :
        sighandlers( nullptr ){}

    struct Process : Next::Process
    {
        int pid;
        void *globals;
    };

    using Thread = __dios::Thread< Process >;

    void setup( MemoryPool& pool, const _VM_Env *env, SysOpts opts ) {
        Process *proc1 = static_cast< Process * >( __vm_obj_make( sizeof( Process ) ) );
        proc1->globals = __vm_control( _VM_CA_Get, _VM_CR_Globals );
        proc1->pid = 1;

        auto mainThr = newThreadMem( pool.get(), pool.get(), _start, 0, proc1 );
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

    void getHelp( Map< String, HelpOption >& options ) {
        const char *opt = "{trace|notrace}";
        if ( options.find( opt ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        };

        options[ opt ] = HelpOption{ "report/not report item back to VM",
            { "threads - thread info during execution"} };
        Next::getHelp( options );
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
    Thread *newThreadMem( void *mainFrame, void *mainTls, void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        __dios_assert_v( routine, "Invalid thread routine" );
        return threads.emplace( mainFrame, mainTls, routine, tls_size, proc );
    }

    template< typename... Args >
    Thread *newThread( void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        __dios_assert_v( routine, "Invalid thread routine" );
        return threads.emplace( routine, tls_size, proc );
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
        if ( tid == __dios_get_thread_handle() )
            __vm_control( _VM_CA_Set, _VM_CR_User2, nullptr );
        bool res = threads.remove( tid );
        __dios_assert_v( res, "Killing non-existing thread" );
    }

    void killProcess( pid_t id ) noexcept  {
        if ( !id ) {

            size_t c = threads.size();

            for ( int i = 0; i < c; ++i )
                delete_object( threads[ i ] );

            threads.erase( threads.begin(), threads.end() );
            __vm_control( _VM_CA_Set, _VM_CR_User2, nullptr );
            // ToDo: Erase processes
            return;
        }

        auto r = std::remove_if( threads.begin(), threads.end(), [&]( Thread *t )
                                 {
                                     if ( t->_tls == __dios_get_thread_handle() )
                                         __vm_control( _VM_CA_Set, _VM_CR_User2, nullptr );
                                     if ( t->_proc->pid == id )
                                     {
                                         delete_object( t );
                                         return true;
                                     }
                                     return false;
                                } );

        threads.erase( r, threads.end() );
        // ToDo: Erase processes
    }

    int sigaction( int sig, const struct ::sigaction *act, struct sigaction *oldact ) {
        if ( sig < 0 || sig > static_cast< int >( sizeof(defhandlers) / sizeof(__dios::sighandler_t) )
            || sig == SIGKILL || sig == SIGSTOP )
        {
            *__dios_get_errno() = EINVAL;
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


    Process* getProcess(  ) {
        auto tid = __dios_get_thread_handle();
        auto thread = threads.find( tid );
        __dios_assert( thread );
        return thread->_proc;
    }

    pid_t getpid( ) {
        return getProcess()->pid;
    }

    _DiOS_ThreadHandle start_thread( _DiOS_ThreadRoutine routine, void * arg, int tls_size ) {
        auto t = newThread( routine, tls_size, getProcess() );
        setupThread( t, arg );
        __vm_obj_shared( t->getId() );
        __vm_obj_shared( arg );
        return t->getId();
    }

    void kill_thread( _DiOS_ThreadHandle id ) {
        killThread( id );
    }

    _DiOS_ThreadHandle *get_process_threads( _DiOS_ThreadHandle tid ) {
        Process *proc;
        for ( auto &t : threads ) {
            if ( t->_tls == tid ) {
                proc = t->_proc;
                break;
            }
        }
        int count = 0;
        for ( auto &t : threads ) {
            if ( t->_proc == proc )
                ++count;
        }
        auto ret = static_cast< _DiOS_ThreadHandle * >( __vm_obj_make( sizeof( _DiOS_ThreadHandle ) * count ) );
        int i = 0;
        for ( auto &t : threads ) {
            if ( t->_proc == proc ) {
                ret[ i ] = t->_tls;
                ++i;
            }
        }
        return ret;
    }

    int kill( pid_t pid, int sig ) {
        sighandler_t handler;
        bool found = false;
        Thread *thread;
        for ( auto t : threads )
            if ( t->_proc->pid == pid )
            {
                found = true;
                thread = t;
                break;
            }
        if ( !found )
        {
            *__dios_get_errno() = ESRCH;
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

    void die() {
        killProcess( 0 );
    }


    pid_t sysfork( void )
    {
        auto tid = __dios_get_thread_handle();
        auto thread = threads.find( tid );
        Thread *newThread = static_cast< Thread * >( __vm_obj_clone( thread ) );

        pid_t maxPid = 0;
        for( auto t : threads )
        {
            if ( t->_proc->pid > maxPid )
                maxPid = t->_proc->pid;
        }

        newThread->_proc->pid = maxPid + 1;
        threads.insert( newThread );
        return newThread->_proc->pid;
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
                          _VM_CA_Set, _VM_CR_Globals, t->_proc->globals,
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
};

#endif

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
