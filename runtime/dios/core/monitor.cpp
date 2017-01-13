// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <cstdarg>
#include <dios.h>
#include <dios/core/monitor.hpp>
#include <dios/core/syscall.hpp>

namespace __sc {
void register_monitor( __dios::Context& ctx, int *, void *, va_list vl ) {
    typedef __dios::Monitor * pMonitor;
    auto *m = va_arg( vl, pMonitor );
    m->next = nullptr;
    if ( !ctx.monitors ) {
        ctx.monitors = m;
        return;
    }

    auto *last = ctx.monitors;
    while ( last->next )
        last = last->next;
    last->next = m;
}

} // namespace __sc

void __dios_register_monitor( _DiOS_Monitor *monitor ) noexcept {
    __dios_syscall( __dios::_SC_register_monitor, nullptr, monitor );
}
