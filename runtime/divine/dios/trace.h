// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#ifndef __DIOS_TRACE_H__
#define __DIOS_TRACE_H__

#include <cstdarg>
#include <divine/dios.h>

namespace __dios {

struct InTrace {
    InTrace() : prev( inTrace ) { inTrace = true; }
    ~InTrace() { inTrace = prev; }

    bool prev;
    static bool inTrace;
};

void traceInternalV( int indent, const char *fmt, va_list ap ) noexcept __attribute__((always_inline));
void traceInternal( int indent, const char *fmt, ... ) noexcept;


} // namespace __dios


#endif // __DIOS_TRACE_H__
