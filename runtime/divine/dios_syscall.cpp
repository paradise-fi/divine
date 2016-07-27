// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include "dios_syscall.h"

void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( void* retval, va_list vl ) = {
    [ _SC_START_THREAD ] = __sc_start_thread,
    [ _SC_GET_THREAD_ID ] = __sc_get_thread_id,
    [ _SC_KILL_THREAD ] = __sc_kill_thread,
    [ _SC_DUMMY ] = __sc_dummy
};