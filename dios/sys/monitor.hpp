#ifndef _DIVINE_MONITOR_HPP_
#define _DIVINE_MONITOR_HPP_

#include <sys/monitor.h>
#include <dios/sys/memory.hpp>

namespace __dios {

template < typename Next >
struct MonitorManager : Next
{
    MonitorManager() : first( nullptr ) {}

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< MonitorManager >( "{Monitor}" );
        Next::setup( s );
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
