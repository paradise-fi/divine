// -*- C++ -*- (c) 2018 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_FAIR_SCHEDULING_H__
#define __DIOS_FAIR_SCHEDULING_H__

#include <dios/sys/scheduling.hpp>

namespace __dios {

template < typename Next >
struct FairScheduler : public Scheduler< Next > {
     using Task = __dios::task< typename Scheduler< Next >::Process >;
     using Process = typename Scheduler< Next >::Process;

    FairScheduler() : _workingGroup( 0 ) {
        _fairGroup.push_back( nullptr );
    }

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< FairScheduler >( "{Scheduler}" );

        s.proc1->globals = __vm_ctl_get( _VM_CR_Globals );
        s.proc1->pid = 1;

        auto mainTask = this->newTaskMem( s.pool->get(), s.pool->get(), _start, 0, s.proc1 );
        _fairGroup.push_back( mainTask->get_id() );
        auto argv = construct_main_arg( "arg.", s.env, true );
        auto envp = construct_main_arg( "env.", s.env );
        this->setupMainTask( mainTask, argv.first, argv.second, envp.second );

        __vm_ctl_set( _VM_CR_Scheduler,
                      reinterpret_cast< void * >( run_scheduler< typename Setup::Context > ) );
        this->setupDebug( s, argv, envp );

        Next::setup( s );
    }

    __dios_task start_task( __dios_task_routine routine, void * arg, int tls_size )
    {
        auto t = this->newTask( routine, tls_size, this->current_process() );
        this->setupTask( t, arg );
        _fairGroup.push_back( t->get_id() );
        return t->get_id();
    }

    void killTask( __dios_task tid ) noexcept
    {
        Scheduler< Next >::killTask( tid );
        auto it = std::find( _fairGroup.begin(), _fairGroup.end(), tid );
        int idx = it - _fairGroup.begin();
        if ( idx >= _workingGroup )
            _workingGroup--;
        _fairGroup.erase( it );
    }

    void moveToNextGroup()
    {
        _workingGroup++;
        if ( _workingGroup == _fairGroup.size() )
            _workingGroup = 0;
    }

    __inline void run( Task &t )
    {
        this->prepare( t );
        this->monitor( t );
        check_fairness();
        this->jump( t );
    }

    template < typename Context >
    static void run_scheduler() noexcept
    {
        auto &scheduler = get_state< Context >();

        scheduler.traceTasks();
        Task *t = scheduler.chooseTask();

        if ( t )
            __vm_trace( _VM_T_TaskID, t );

        __vm_ctl_flag( 0, _DiOS_CF_Fairness );

        if ( t && t->_frame )
            scheduler.run( *t );

        __vm_cancel();
    }

    void check_fairness() /* figure out fairness constraints */
    {
        bool accepting = __vm_ctl_flag( 0, 0 ) & _VM_CF_Accepting;

        if ( _workingGroup == 0 && accepting )
            moveToNextGroup();
        else if ( _workingGroup != 0 )
        {
            if ( _fairGroup[ _workingGroup ] == __dios_this_task() )
                moveToNextGroup();
            __vm_ctl_flag( /* clear */ _VM_CF_Accepting, 0 );
        }
    }

    __dios::Array< __dios_task > _fairGroup;
    int _workingGroup;
};

} // namespace __dios

#endif // __DIOS_FAIR_SCHEDULING_H
