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

#include <dios/sys/main.hpp>
#include <dios/sys/syscall.hpp>
#include <util/map.hpp>

#include <rst/common.h>

namespace __dios {

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

struct TrampolineFrame : _VM_Frame
{
    void *arg1, *arg2;
    int rv;
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
struct Task : KObject
{
    _VM_Frame *_frame;
    __dios_tls *_tls;
    Process *_proc;
    const _MD_Function *_fun;

    template <class F>
    Task( F routine, int tls_size, Process *proc ) noexcept
        : _frame( nullptr ),
          _proc( proc ),
          _fun( __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) ) )
    {
        setup_stack();
        _tls = static_cast< __dios_tls * >( __vm_obj_make( sizeof( __dios_tls ) + tls_size ) );
        _tls->__errno = 0;
    }

    template <class F>
    Task( void *mainFrame, void *mainTls, F routine, int tls_size, Process *proc ) noexcept
        : _frame( nullptr ),
          _proc( proc ),
          _fun( __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( routine ) ) )
    {
        setup_stack( mainFrame );
        _tls = static_cast< __dios_tls * >( mainTls );
        __vm_obj_resize( _tls, sizeof( __dios_tls ) + tls_size );
        _tls->__errno = 0;
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

    ~Task() noexcept
    {
        free_stack();
        __vm_obj_free( _tls );
    }

    bool active() const noexcept { return _frame; }
    __dios_task getId() const noexcept { return _tls; }
    uint32_t getUserId() const noexcept {
        auto tid = reinterpret_cast< uint64_t >( _tls ) >> 32;
        return static_cast< uint32_t >( tid );
    }

    void setup_stack( void *frame = nullptr ) noexcept
    {
        free_stack();
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

    void free_stack() noexcept
    {
        if ( _frame )
            /* if the destructed part of stack is usperspace end of this stack,
             * we need to pass the actual top of this stack as the first
             * argument to make sure the stack does not end with a dangling
             * pointer */
            __dios_unwind( _tls == __dios_this_task() ? __dios_this_frame() : _frame,
                           _frame, nullptr );
        _frame = nullptr;
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
        virtual ~Process()
        {
            __vm_obj_free( globals );
        }
    };

    using Task = __dios::Task< Process >;
    using Tasks = TaskStorage< Task >;

    Scheduler() :
        debug( new Debug() ),
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
    void setup( Setup s )
    {
        traceAlias< Scheduler >( "{Scheduler}" );

        s.proc1->globals = __vm_ctl_get( _VM_CR_Globals );
        s.proc1->pid = 1;

        auto mainTask = newTaskMem( s.pool->get(), s.pool->get(), _start, 0, s.proc1 );
        auto argv = construct_main_arg( "arg.", s.env, true );
        auto envp = construct_main_arg( "env.", s.env );
        setupMainTask( mainTask, argv.first, argv.second, envp.second );

        __vm_ctl_set( _VM_CR_Scheduler,
                      reinterpret_cast< void * >( run_scheduler< typename Setup::Context > ) );
        setupDebug( s, argv, envp );

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

    __debugfn void traceTasks() const noexcept
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
                debug->persist();
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
        Task *ret = tasks.emplace_back( new Task( mainFrame, mainTls, routine, tls_size, proc ) ).get();
        sortTasks();
        return ret;
    }

    template< typename... Args >
    Task *newTask( void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        __dios_assert_v( routine, "Invalid task routine" );
        Task *ret = tasks.emplace_back( new Task( routine, tls_size, proc ) ).get();
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

    void killTask( __dios_task tid ) noexcept
    {
        bool res = tasks.remove( tid );
        __dios_assert_v( res, "Killing non-existing task" );
        if ( tid == __dios_this_task() )
            this->reschedule();
    }

    template< typename I >
    void eraseProcesses( I pivot )
    {
        ArrayMap< Process *, bool > p;
        for ( auto i = pivot; i != tasks.end(); ++i )
            p.emplace( (*i)->_proc, true );
        for ( auto i = tasks.begin(); i != pivot; ++i )
            p.erase( (*i)->_proc );
        for ( auto item : p )
            delete item.first;
    }

    void killProcess( pid_t id ) noexcept
    {
        if ( !id )
        {
            size_t c = tasks.size();
            eraseProcesses( tasks.begin() );
            tasks.erase( tasks.begin(), tasks.end() );
            this->reschedule();
        }

        bool resched = false;
        auto r = std::partition( tasks.begin(), tasks.end(), [&]( auto& t )
                                 {
                                     if ( t->_proc->pid != id )
                                         return true;
                                     if ( t->_tls == __dios_this_task() )
                                         resched = true;
                                     return false;
                                 } );

        eraseProcesses( r );
        tasks.erase( r, tasks.end() );
        if ( resched )
            this->reschedule();
    }

    int sigaction( int sig, const struct ::sigaction *act, struct sigaction *oldact )
    {
        if ( sig <= 0 || sig > static_cast< int >( sizeof(defhandlers) / sizeof(__dios::sighandler_t) )
            || sig == SIGKILL || sig == SIGSTOP )
        {
            *__dios_errno() = EINVAL;
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

    int rt_sigaction( int sig, const struct ::sigaction *act, struct sigaction *oldact, size_t sz )
    {
        if ( sz == sizeof( sigset_t ) )
            return sigaction( sig, act, oldact );
        __dios_fault( _VM_F_NotImplemented, "rt_sigaction with size != sizeof( sigset_t )" );
    }

    Task* getCurrentTask() {
        auto tid = __dios_this_task();
        return tasks.find( tid );
    }

    pid_t getpid() {
        return getCurrentTask()->_proc->pid;
    }

    __dios_task start_task( __dios_task_routine routine, void * arg, int tls_size )
    {
        auto t = newTask( routine, tls_size, getCurrentTask()->_proc );
        setupTask( t, arg );
        return t->getId();
    }

    void exit_process( int code )
    {
        die();
    }

    void kill_task( __dios_task id )
    {
        killTask( id );
    }

    __dios_task *get_process_tasks( __dios_task tid )
    {
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
        auto ret = static_cast< __dios_task * >( __vm_obj_make( sizeof( __dios_task ) * count ) );
        int i = 0;
        for ( auto &t : tasks ) {
            if ( t->_proc == proc ) {
                ret[ i ] = t->_tls;
                ++i;
            }
        }
        return ret;
    }

    template< typename F >
    _VM_Frame *mkframe( F f )
    {
        auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( f ) );
        auto frame = static_cast< _VM_Frame * >( __vm_obj_make( fun->frame_size ) );
        frame->pc = reinterpret_cast< _VM_CodePointer >( f );
        return frame;
    }

    _VM_Frame *sysenter( bool skip_invoke = true )
    {
        _VM_Frame *f = static_cast< _VM_Frame * >( __vm_ctl_get( _VM_CR_Frame ) );
        while ( !__md_get_pc_meta( f->pc )->is_trap )
            f = f->parent;
        /* sysenter goes through a function pointer to a lambda, i.e. __invoke */
        return skip_invoke ? f->parent : f;
    }

    int trampoline_return( int rv )
    {
        auto ltf = static_cast< TrampolineFrame * >( mkframe( __dios_simple_trampoline ) );
        ltf->rv = rv;
        ltf->parent = sysenter()->parent;
        sysenter()->parent = ltf;
        return rv;
    }

    void trampoline_to( Task *task,
                        int (*trampoline_ret)( void *, void *, int ),
                        int (*trampoline_noret)( void *, void *, int ),
                        int rv, void *arg1, void *arg2 )
    {
        if ( task->_tls == __dios_this_task() )
        {
            auto tf = static_cast< TrampolineFrame * >( mkframe( trampoline_ret ) );
            tf->parent = sysenter()->parent;
            tf->rv = rv;
            tf->arg1 = arg1;
            tf->arg2 = arg2;
            sysenter()->parent = tf;
        }
        else
        {
            auto tf = static_cast< TrampolineFrame * >( mkframe( trampoline_noret ) );
            tf->arg1 = arg1;
            tf->arg2 = arg2;
            tf->parent = task->_frame;
            task->_frame = tf;
            trampoline_return( rv );
        }
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
            *__dios_errno() = ESRCH;
            return trampoline_return( -1 );
        }
        if ( sighandlers )
            handler = sighandlers[sig];
        else
            handler = defhandlers[sig];
        if ( handler.f == sig_ign )
            return trampoline_return( 0 );
        if ( handler.f == sig_die )
        {
            func( task->_proc );
            killProcess( pid );
        }
        else if ( handler.f == sig_fault )
            __dios_fault( _VM_F_Control, "Uncaught signal." );
        else
        {
            trampoline_to( task, __dios_signal_trampoline_ret, __dios_signal_trampoline_noret, 0,
                           reinterpret_cast< void * >( handler.f ),
                           reinterpret_cast< void * >( sig ) );
            return 0;
        }

        return trampoline_return( 0 );
    }

    void die() noexcept { killProcess( 0 ); }

    bool check_final()
    {
        return tasks.empty();
    }

    __inline void prepare( Task &t )
    {
        __vm_ctl_set( _VM_CR_Globals, t._proc->globals );
        __vm_ctl_set( _VM_CR_User1, &t._frame );
        __vm_ctl_set( _VM_CR_User2, t.getId() );
        __vm_ctl_set( _VM_CR_User3, debug );
    }

    __inline void monitor( Task &t )
    {
        this->runMonitors();
    }

    __inline void jump( Task &t )
    {
        auto f = t._frame;
        __vm_ctl_flag( _VM_CF_KernelMode | _VM_CF_IgnoreCrit | _VM_CF_IgnoreLoop, 0 );
        __vm_ctl_set( _VM_CR_Frame, f );
    }

    __inline void run( Task &t )
    {
        prepare( t );
        monitor( t ); /* hmm. */
        jump( t );
    }

    template < typename Context >
    static void run_scheduler() noexcept
    {
        auto& scheduler = get_state< Context >();

        scheduler.traceTasks();
        Task *t = scheduler.chooseTask();

        if ( t )
            __vm_trace( _VM_T_TaskID, t );

        if ( t && t->_frame )
            scheduler.run( *t ); /* does not return */

        __vm_cancel();
    }

    Tasks tasks;
    Debug *debug;
    sighandler_t *sighandlers;
};

#endif

} // namespace __dios


#endif // __DIOS_SCHEDULING_H__
