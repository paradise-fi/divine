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
#include <sys/metadata.h>
#include <dios.h>

#include <dios/core/main.hpp>
#include <dios/core/syscall.hpp>
#include <dios/lib/map.hpp>

#include <abstract/common.h>

namespace __dios {

using TaskHandle = _DiOS_TaskHandle;

struct DiosMainFrame : _VM_Frame {
    int l;
    int argc;
    char** argv;
    char** envp;
};

struct TaskRoutineFrame : _VM_Frame {
    void* arg;
};

struct CleanupFrame : _VM_Frame {
    int reason;
};

struct TrampolineFrame : _VM_Frame {
    _VM_Frame * interrupted;
    void ( *handler )( int );
};

template < typename T >
struct TaskStorage: Array< std::unique_ptr< T > > {
    using Tid = decltype( std::declval< T >().getId() );

    T *find( Tid id ) noexcept {
        for ( auto& t : *this )
            if ( t->getId() == id )
                return t.get();
        return nullptr;
    }

    bool remove( Tid id ) noexcept {
        for ( auto& t : *this ) {
            if ( t ->getId() != id )
                continue;
            std::swap( t, this->back() );
            this->pop_back();
            return true;
        }
        return false;
    }
};

template < typename Process >
struct Task {
    _VM_Frame *_frame;
    struct _DiOS_TLS *_tls;
    Process *_proc;
    const _MD_Function *_fun;

    template <class F>
    Task( F routine, int tls_size, Process *proc ) noexcept
        : _frame( nullptr ),
          _proc( proc ),
          _fun( __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) ) )
    {
        setupFrame();
        _tls = reinterpret_cast< struct _DiOS_TLS * >
            ( __vm_obj_make( sizeof( struct _DiOS_TLS ) + tls_size ) );
        _tls->_errno = 0;
    }

    template <class F>
    Task( void *mainFrame, void *mainTls, F routine, int tls_size, Process *proc ) noexcept
        : _frame( nullptr ),
          _proc( proc ),
          _fun( __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) ) )
    {
        setupFrame( mainFrame );
        _tls = reinterpret_cast< struct _DiOS_TLS * >( mainTls );
        __vm_obj_resize( _tls, sizeof( struct _DiOS_TLS ) + tls_size );
        _tls->_errno = 0;
    }

    Task( const Task& o ) noexcept = delete;
    Task& operator=( const Task& o ) noexcept = delete;

    Task( Task&& o ) noexcept
        : _frame( o._frame ), _tls( o._tls ), _proc( o._proc )
    {
        o._frame = nullptr;
        o._tls = nullptr;
    }

    Task& operator=( Task&& o ) noexcept {
        std::swap( _frame, o._frame );
        std::swap( _tls, o._tls );
        std::swap( _proc, o._proc );
        return *this;
    }

    ~Task() noexcept {
        clearFrame();
        __vm_obj_free( _tls );
    }

    bool active() const noexcept { return _frame; }
    TaskHandle getId() const noexcept { return _tls; }
    uint32_t getUserId() const noexcept {
        auto tid = reinterpret_cast< uint64_t >( _tls ) >> 32;
        return static_cast< uint32_t >( tid );
    }

    void setupFrame( void *frame = nullptr ) noexcept {
        clearFrame();
        if ( frame ) {
            _frame = static_cast< _VM_Frame * >( frame );
            __vm_obj_resize( _frame, _fun->frame_size );
        }
        else
            _frame = static_cast< _VM_Frame * >( __vm_obj_make( _fun->frame_size ) );
        _frame->pc = _fun->entry_point;
        _frame->parent = nullptr;
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
struct Scheduler : public Next
{
    struct Process : Next::Process
    {
        uint16_t pid;
        void *globals;
    };

    using Task = __dios::Task< Process >;
    using Tasks = TaskStorage< Task >;

    Scheduler() :
        debug( new_object< Debug >() ),
        sighandlers( nullptr )
    {
        debug = abstract::weaken( debug );
    }

    ~Scheduler()
    {
        delete_object( debug );
    }

    void sortTasks() {
        if ( tasks.empty() )
            return;
        std::sort( tasks.begin(), tasks.end(), []( const auto& a, const auto& b ) {
            return a->getId() < b->getId();
        });
    }

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< Scheduler >( "{Scheduler}" );

        s.proc1->globals = __vm_control( _VM_CA_Get, _VM_CR_Globals );
        s.proc1->pid = 1;

        auto mainTask = newTaskMem( s.pool->get(), s.pool->get(), _start, 0, s.proc1 );
        auto argv = construct_main_arg( "arg.", s.env, true );
        auto envp = construct_main_arg( "env.", s.env );
        setupMainTask( mainTask, argv.first, argv.second, envp.second );

        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, run_scheduler< typename Setup::Context > );
        setupDebug( s, argv, envp );
        environ = envp.second;

        Next::setup( s );
    }

    template < typename Setup >
    void setupDebug( Setup s, std::pair< int, char** >& argv, std::pair< int, char** >& envp )
    {
        if ( extractOpt( "debug", "mainargs", s.opts ) ||
             extractOpt( "debug", "mainarg", s.opts ) )
        {
            trace_main_arg( 1, "main argv", argv );
            trace_main_arg( 1, "main envp", envp );
        }
    }

    int taskCount() const noexcept { return tasks.size(); }
    Task *chooseTask() noexcept
    {
        if ( tasks.empty() )
            return nullptr;
        return tasks[ __vm_choose( tasks.size() ) ].get();
    }

    __attribute__(( noinline, __annotate__( "divine.debugfn" ) ))
    void traceTasks() const noexcept
    {
        int c = tasks.size();
        if ( c == 0 )
            return;
        struct PI { int pid, tid; unsigned choice; };
        PI *pi = reinterpret_cast< PI * >( __vm_obj_make( c * sizeof( PI ) ) );
        PI *pi_it = pi;

        for ( int i = 0; i != c; i++ )
        {
            pi_it->pid = 0;
            auto tid = abstract::weaken( tasks[ i ]->getId() );
            auto tidhid = debug->hids.find( tid );
            if ( tidhid != debug->hids.end() )
                pi_it->tid = tidhid->second;
            else
            {
                pi_it->tid = debug->hids.push( tid );
                __vm_trace( _VM_T_DebugPersist, debug );
            }
            pi_it->choice = i;
            ++pi_it;
        }

        std::sort( pi, pi + c, []( const PI& a, const PI& b ) {
            if ( a.pid == b.pid )
                return a.tid < b.tid;
            return a.pid < b.pid;
        } );

        __vm_trace( _VM_T_SchedChoice, pi );
        __vm_obj_free( pi );
    }

    template< typename... Args >
    Task *newTaskMem( void *mainFrame, void *mainTls, void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        __dios_assert_v( routine, "Invalid task routine" );
        Task *ret = tasks.emplace_back(
            new_object< Task >( mainFrame, mainTls, routine, tls_size, proc ) ).get();
        sortTasks();
        return ret;
    }

    template< typename... Args >
    Task *newTask( void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        __dios_assert_v( routine, "Invalid task routine" );
        Task *ret = tasks.emplace_back(
            new_object< Task >( routine, tls_size, proc ) ).get();
        sortTasks();
        return ret;
    }

    void setupMainTask( Task * t, int argc, char** argv, char** envp ) noexcept {
        DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( t->_frame );
        frame->l = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( main ) )->arg_count;

        frame->argc = argc;
        frame->argv = argv;
        frame->envp = envp;
    }

    void setupTask( Task * t, void *arg ) noexcept {
        TaskRoutineFrame *frame = reinterpret_cast< TaskRoutineFrame * >( t->_frame );
        frame->arg = arg;
    }

    void killTask( TaskHandle tid ) noexcept  {
        if ( tid == __dios_get_task_handle() )
            reschedule();
        bool res = tasks.remove( tid );
        __dios_assert_v( res, "Killing non-existing task" );
    }

    template< typename I >
    void eraseProcesses( I pivot )
    {
        Set< Process * > p;
        for ( auto i = pivot; i != tasks.end(); ++i )
            p.insert( (*i)->_proc );
        for ( auto i = tasks.begin(); i != pivot; ++i )
            p.erase( (*i)->_proc );
        for ( auto &proc : p )
            delete_object( proc );
    }

    void reschedule()
    {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _DiOS_CF_Reschedule, _DiOS_CF_Reschedule );
    }

    bool need_reschedule()
    {
        return uint64_t( __vm_control( _VM_CA_Get, _VM_CR_Flags ) ) & _DiOS_CF_Reschedule;
    }

    void killProcess( pid_t id ) noexcept  {
        if ( !id ) {

            size_t c = tasks.size();
            eraseProcesses( tasks.begin() );
            tasks.erase( tasks.begin(), tasks.end() );
            reschedule();
            return;
        }

        auto r = std::partition( tasks.begin(), tasks.end(), [&]( auto& t )
                                 {
                                     if ( t->_proc->pid != id )
                                         return true;
                                     if ( t->_tls == __dios_get_task_handle() )
                                         reschedule();
                                     return false;
                                 } );

        eraseProcesses( r );
        tasks.erase( r, tasks.end() );
    }

    int sigaction( int sig, const struct ::sigaction *act, struct sigaction *oldact ) {
        if ( sig <= 0 || sig > static_cast< int >( sizeof(defhandlers) / sizeof(__dios::sighandler_t) )
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

    Task* getCurrentTask() {
        auto tid = __dios_get_task_handle();
        return tasks.find( tid );
    }

    pid_t getpid() {
        return getCurrentTask()->_proc->pid;
    }

    _DiOS_TaskHandle start_task( _DiOS_TaskRoutine routine, void * arg, int tls_size ) {
        auto t = newTask( routine, tls_size, getCurrentTask()->_proc );
        setupTask( t, arg );
        __vm_obj_shared( t->getId() );
        __vm_obj_shared( arg );
        return t->getId();
    }

    void exit_process( int code )
    {
        die();
    }

    void kill_task( _DiOS_TaskHandle id ) {
        killTask( id );
    }

    _DiOS_TaskHandle *get_process_tasks( _DiOS_TaskHandle tid ) {
        Process *proc;
        for ( auto &t : tasks ) {
            if ( t->_tls == tid ) {
                proc = t->_proc;
                break;
            }
        }
        int count = 0;
        for ( auto &t : tasks ) {
            if ( t->_proc == proc )
                ++count;
        }
        auto ret = static_cast< _DiOS_TaskHandle * >( __vm_obj_make( sizeof( _DiOS_TaskHandle ) * count ) );
        int i = 0;
        for ( auto &t : tasks ) {
            if ( t->_proc == proc ) {
                ret[ i ] = t->_tls;
                ++i;
            }
        }
        return ret;
    }

    int kill( pid_t pid, int sig )
    {
        return _kill( pid, sig, []( auto ){} );
    }

    template < typename F >
    int _kill( pid_t pid, int sig, F func ) {
        sighandler_t handler;
        bool found = false;
        Task *task;
        for ( auto& t : tasks )
            if ( t->_proc->pid == pid )
            {
                found = true;
                task = t.get();
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
        {
            func( task->_proc );
            killProcess( pid );
        }
        else if ( handler.f == sig_fault )
            __dios_fault( _VM_F_Control, "Uncaught signal." );
        else
        {
            auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline ) );
            auto _frame = static_cast< __dios::TrampolineFrame * >( __vm_obj_make( fun->frame_size ) );
            _frame->pc = reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline );
            _frame->interrupted = task->_frame;
            task->_frame = _frame;
            _frame->parent = nullptr;
            _frame->handler = handler.f;
        }
        return 0;
    }

    void die() noexcept { killProcess( 0 ); }

    template< typename Context >
    static void check_final( Context &ctx )
    {
        if ( ctx.tasks.empty() )
            ctx.finalize();
    }

    __attribute__((__always_inline__))
    void run( Task &t )
    {
        __vm_control( _VM_CA_Set, _VM_CR_Frame, t._frame,
                      _VM_CA_Set, _VM_CR_Globals, t._proc->globals,
                      _VM_CA_Set, _VM_CR_User2, reinterpret_cast< uint64_t >( t.getId() ),
                      _VM_CA_Set, _VM_CR_User3, debug,
                      _VM_CA_Bit, _VM_CR_Flags,
                      uintptr_t( _VM_CF_Interrupted | _VM_CF_Mask | _VM_CF_KernelMode ), 0ull );
        t._frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_IntFrame ) );
    }

    template < typename Context >
    static void run_scheduler() noexcept
    {
        void *ctx = __vm_control( _VM_CA_Get, _VM_CR_State );
        auto& scheduler = *static_cast< Context * >( ctx );
        using Sys = Syscall< Context >;

        scheduler.traceTasks();
        Task *t = scheduler.chooseTask();

        if ( t )
            __vm_trace( _VM_T_TaskID, t );

        while ( t && t->_frame )
        {
            scheduler.run( *t );
            scheduler.runMonitors();

            auto syscall = static_cast< _DiOS_Syscall * >( __vm_control( _VM_CA_Get, _VM_CR_User1 ) );
            if ( syscall )
            {
                Sys::handle( scheduler, *syscall );
                __vm_control( _VM_CA_Set, _VM_CR_User1, nullptr );
            }
            else
            {
                if ( !t->_frame )
                    check_final( scheduler );
                __vm_suspend();
            }

            if ( scheduler.need_reschedule() )
                check_final( scheduler ), __vm_suspend();

            /* reset intframe to ourselves */
            auto self = __vm_control( _VM_CA_Get, _VM_CR_Frame );
            __vm_control( _VM_CA_Set, _VM_CR_IntFrame, self );
        }
        __vm_cancel();
    }

    Tasks tasks;
    Debug *debug;
    Map< pid_t, Process* > zombies;
    sighandler_t *sighandlers;
};

#endif

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
