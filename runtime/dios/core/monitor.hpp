#ifndef _DIVINE_MONITOR_HPP_
#define _DIVINE_MONITOR_HPP_

#include <sys/monitor.h>
#include <dios/core/memory.hpp>

namespace __dios {

template < typename Next >
struct MonitorManager : Next
{
    MonitorManager() : first( nullptr ) {}

    void setup( MemoryPool& pool, const _VM_Env *env, const SysOpts& opts )
    {
        Next::setup( pool, env, opts );
    }

    void runMonitors()
    {
        if ( first )
            first->run();
    }

    void register_monitor( __dios::Monitor *m )
    {
        m->next = nullptr;
        if ( !first ) {
            first = m;
            return;
        }

        auto *last = first;
        while ( last->next )
            last = last->next;
        last->next = m;
    }

    Monitor* first;
};

} // namespace __dios

#endif // _DIVINE_MONITOR_HPP
