// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <cstdarg>
#include <dios.h>
#include <sys/monitor.h>
#include <dios/core/syscall.hpp>
#include <dios/kernel.hpp>

namespace __sc {

void register_monitor( __dios::Context& ctx, int *, void *, va_list vl )
{
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

}

namespace __sc_passthru {

void register_monitor( __dios::Context& ctx, int *err, void *ret, va_list vl )
{
    __sc::register_monitor(ctx, err, ret , vl);
}

}

