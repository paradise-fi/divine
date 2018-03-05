// -*- C++ -*- (c) 2018 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_FAIR_SCHEDULING_H__
#define __DIOS_FAIR_SCHEDULING_H__

#include <dios/core/scheduling.hpp>

namespace __dios {

template < typename Next >
struct FairScheduler : public Scheduler< Next > {
     using Task = __dios::Task< typename Scheduler< Next >::Process >;
     using Process = typename Scheduler< Next >::Process;

    FairScheduler() : _workingGroup( 0 ) {
        _fairGroup.push_back( nullptr );
    }

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< FairScheduler >( "{Scheduler}" );

        s.proc1->globals = __vm_control( _VM_CA_Get, _VM_CR_Globals );
        s.proc1->pid = 1;

        auto mainTask = newTaskMem( s.pool->get(), s.pool->get(), _start, 0, s.proc1 );
        auto argv = construct_main_arg( "arg.", s.env, true );
        auto envp = construct_main_arg( "env.", s.env );
        this->setupMainTask( mainTask, argv.first, argv.second, envp.second );

        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, run_scheduler< typename Setup::Context > );
        this->setupDebug( s, argv, envp );
        environ = envp.second;

        Next::setup( s );
    }

    template< typename... Args >
    Task *newTaskMem( void *mainFrame, void *mainTls, void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        Task *t = Scheduler< Next >::newTaskMem( mainFrame, mainTls, routine, tls_size, proc );
        _fairGroup.push_back( t->getId() );
        return t;
    }

    template< typename... Args >
    Task *newTask( void ( *routine )( Args... ), int tls_size, Process *proc ) noexcept
    {
        Task *t = Scheduler< Next >::newTask( routine, tls_size, proc );
        _fairGroup.push_back( t->getId() );
        return t;
    }

    __dios_task start_task( __dios_task_routine routine, void * arg, int tls_size )
    {
        auto t = newTask( routine, tls_size, this->getCurrentTask()->_proc );
        this->setupTask( t, arg );
        __vm_obj_shared( t->getId() );
        __vm_obj_shared( arg );
        return t->getId();
    }

    void killTask( __dios_task tid ) noexcept {
        Scheduler< Next >::killTask( tid );
        auto it = std::find( _fairGroup.begin(), _fairGroup.end(), tid );
        int idx = it - _fairGroup.begin();
        if ( idx >= _workingGroup )
            _workingGroup--;
        _fairGroup.erase( it );
    }

    void moveToNextGroup() {
        _workingGroup++;
        if ( _workingGroup == _fairGroup.size() )
            _workingGroup = 0;
    }

    template < typename Context >
    static void run_scheduler() noexcept
    {
        void *ctx = __vm_control( _VM_CA_Get, _VM_CR_State );
        auto& scheduler = *static_cast< Context * >( ctx );

        scheduler.traceTasks();
        Task *t = scheduler.chooseTask();

        if ( t )
            __vm_trace( _VM_T_TaskID, t );

        while ( t && t->_frame )
        {
            scheduler.run( *t );
            scheduler.runMonitors();

            // Fairness contraint
            bool accepting = uint64_t( __vm_control( _VM_CA_Get, _VM_CR_Flags ) ) & _VM_CF_Accepting;

            if ( scheduler._workingGroup == 0 && accepting )
            {
                scheduler.moveToNextGroup();
            }
            else if ( scheduler._workingGroup != 0 )
            {
                if ( scheduler._fairGroup[ scheduler._workingGroup ] == t->getId() ) {
                    scheduler.moveToNextGroup();
                }
                __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Accepting, 0 );
            }

            if ( !t->_frame )
                Scheduler< Next >::check_final( scheduler );
            __vm_suspend();
        }
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
    }

    __dios::Array< __dios_task > _fairGroup;
    int _workingGroup;
};

} // namespace __dios

#endif // __DIOS_FAIR_SCHEDULING_H
