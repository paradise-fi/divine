#include <string.h>
#include <errno.h>
#include <sys/wait.h>

namespace __dios
{

template < typename Next >
struct ProcessManager : public Next
{
    struct Process : Next::Process
    {
        uint16_t ppid;
        uint16_t sid;
        uint16_t pgid;
        uint16_t exitStatus = 0;
        bool calledExecve = false;
    };

    using Task = typename Next::Task;

    Process* proc( Task *t )
    {
        return static_cast< Process* >( t->_proc );
    }

    template < typename P >
    Process* proc( P *p )
    {
        return static_cast< Process* >( p );
    }

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< ProcessManager >( "{ProcMan}" );
        s.proc1->ppid = 0;
        s.proc1->sid = 1;
        s.proc1->pgid = 1;
        bool calledExecve = false;
        Next::setup( s );
    }

    bool isChild( Process* child, Process* parent )
    {
        if (!child || !parent)
            return false;
        return child->ppid == parent->pid;
    }

    Process* findProcess( pid_t pid )
    {
        auto task = std::find_if( this->tasks.begin(), this->tasks.end(), [&]( auto& t )
                                     { return proc(t.get())->pid == pid; } );
        if ( task == this->tasks.end() )
        {
            *__dios_errno() = ESRCH;
            return nullptr;
        }
        return proc(task->get());
    }

    pid_t getppid()
    {
        return proc(this->getCurrentTask())->ppid;
    }

    pid_t getsid( pid_t pid )
    {
        if ( pid == 0 )
            return proc(this->getCurrentTask())->sid;
        Process* proc = this->findProcess( pid );
        return proc ? proc->sid : -1;
    }

    pid_t setsid()
    {
        Process* p = proc(this->getCurrentTask());
        for( auto& t : this->tasks )
            if( proc(t.get())->sid == p->sid && proc(t.get())->pgid == p->pid )
            {
                *__dios_errno() = EPERM;
                return -1;
            }
        p->sid = p->pid;
        p->pgid = p->pid;
        return p->sid;
    }

    pid_t getpgid( pid_t pid )
    {
        if ( pid == 0 )
            return proc(this->getCurrentTask())->pgid;
        Process* proc = this->findProcess( pid );
        return proc ? proc->pgid : -1;
    }

    int setpgid( pid_t pid, pid_t pgid )
    {
        if ( pgid < 0 )
        {
            *__dios_errno() = EINVAL;
            return -1;
        }

        Process* procToSet;
        Process* currentProc = proc(this->getCurrentTask());
        if ( pid == 0 )
            procToSet = currentProc;
        else
        {
            procToSet = this->findProcess( pid );
            if ( !procToSet )
                return -1;
            if ( !isChild( procToSet, currentProc ) && pid != currentProc->pid )
            {
                *__dios_errno() = ESRCH;
                return -1;
            }
        }

        if ( procToSet->pgid == procToSet->pid ||
             ( isChild( procToSet, currentProc ) && procToSet->sid != currentProc->sid ) )
        {
            *__dios_errno() = EPERM;
            return -1;
        }
        if ( std::find_if( this->tasks.begin(), this->tasks.end(), [&]( auto& t )
                          { return proc(t.get())->pgid == pgid && proc(t.get())->sid == procToSet->sid; } )
            == this->tasks.end() )
            if ( procToSet->pid != pgid && pgid != 0 )
            {
                *__dios_errno() = EPERM;
                return -1;
            }

        if ( isChild( procToSet, currentProc ) && procToSet->calledExecve )
        {
            *__dios_errno() = EACCES;
            return -1;
        }
        if ( pgid == 0 )
            procToSet->pgid = procToSet->pid;
        else
            procToSet->pgid = pgid;

        return 0;
    }

    pid_t getpgrp()
    {
        return getpgid( 0 );
    }

    int setpgrp()
    {
        return setpgid( 0, 0 );
    }

    struct Clone
    {
        Task *task;
        void *globals;
    };

    void sysfork( pid_t *child )
    {
        auto tid = __dios_this_task();
        auto oldtask = this->tasks.find( tid );
        __dios_assert( oldtask );

        auto oldproc = proc( oldtask );

        pid_t maxPid = 0;
        for( auto& t : this->tasks )
        {
            if ( proc( t.get() )->pid > maxPid )
                maxPid = proc( t.get() )->pid;
        }

        *child = maxPid + 1;

        oldtask->_frame = this->sysenter()->parent;
        oldtask->_proc = nullptr;

        Clone *oldclone = new ( nofail ) Clone, *newclone;
        oldclone->task = oldtask;
        oldclone->globals = oldproc->globals;
        newclone = static_cast< Clone * >( __vm_obj_clone( oldclone ) );

        Process *newproc = static_cast< Process * >( this->make_process( oldproc ) );
        Task *newtask = newclone->task;

        oldtask->_proc = oldproc;
        newtask->_proc = newproc;

        newproc->globals = newclone->globals;
        newproc->pid  = maxPid + 1;
        newproc->ppid = oldproc->pid;
        newproc->sid  = oldproc->sid;
        newproc->pgid = oldproc->pgid;

        delete oldclone;
        delete newclone;

        this->tasks.emplace_back( newtask );
    }

    pid_t wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage)
    {
        if ( options & ~( WNOHANG | WUNTRACED | WCONTINUED ) )
        {
            *__dios_errno() = EINVAL;
            return -1;
        }

        if ( rusage )
            memset( rusage, 0, sizeof( struct rusage ) );

        Process* parent = proc( this->tasks.find( __dios_this_task() ) );
        pid_t childpid;

        auto pid_criteria_func = [&]( auto& pr )
        {
            Process* p = proc( pr );

            if ( p->ppid != parent->pid )
                return false;
            if ( pid < -1 )
                return p->pgid == abs( pid );
            if ( pid == -1 )
                return true;
            if ( pid == 0 )
                return p->pgid == parent->pgid;
            return p->pid == pid;
        };

        auto child = std::find_if( this->zombies.begin(), this->zombies.end(),
                                   [&]( auto& p ) { return pid_criteria_func( p.second ); } );

        if ( child == this->zombies.end() )
        {
            if ( options & WNOHANG )
            {
                *__dios_errno() = ECHILD;
                if ( std::count_if( this->tasks.begin(), this->tasks.end(), [&]( auto& pr ) {
                    return pid_criteria_func( pr->_proc );
                } ) )
                    return 0;
                else
                    return -1;
            }
            else
                __vm_cancel();
        }
        else
        {
            Process *p = proc( child->second );
            if ( wstatus )
                *wstatus = p->exitStatus;
            childpid = p->pid;
            delete_object( p );
            this->zombies.erase( pid );
        }
        return childpid;
    }

    int kill( pid_t pid, int sig )
    {
        return Next::_kill( pid, sig,
                            [&]( auto* p ) { proc( p )->exitStatus = sig; } );
    }

    void exit_process( int code )
    {
        Process* p = proc( this->tasks.find( __dios_this_task() ) );
        p->exitStatus = code << 8;
        Next::killProcess( p->pid );
    }

    Map< pid_t, Process* > zombies;
};

} //namespace __dios
