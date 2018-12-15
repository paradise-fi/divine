// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#ifndef __DIOS_TRACE_H__
#define __DIOS_TRACE_H__

#include <cstdarg>
#include <dios.h>

namespace __dios {

void traceInternal( int indent, const char *fmt, ... ) noexcept;


} // namespace __dios


#endif // __DIOS_TRACE_H__
