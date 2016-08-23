// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <divine/dios/scheduling.h>

__dios::ControlFlow *__dios::Scheduler::_cf;

_DiOS_ThreadId __dios_start_thread( _DiOS_FunPtr routine, void *arg,
    _DiOS_FunPtr cleanup ) noexcept
{
    _DiOS_ThreadId ret;
    __dios_syscall( _SC_START_THREAD, &ret, routine, arg, cleanup );
    return ret;
}

_DiOS_ThreadId __dios_get_thread_id() noexcept {
    _DiOS_ThreadId ret;
    __dios_syscall( _SC_GET_THREAD_ID, &ret );
    return ret;
}

void __dios_kill_thread( _DiOS_ThreadId id, int reason ) noexcept {
    __dios_syscall( _SC_KILL_THREAD, nullptr, id, reason );
}

void __sc_start_thread( void *retval, va_list vl ) {
    auto routine = va_arg( vl, __dios::FunPtr );
    auto arg = va_arg( vl, void * );
    auto cleanup = va_arg( vl, __dios::FunPtr );
    auto ret = static_cast< __dios::ThreadId * >( retval );

    *ret = __dios::Scheduler().start_thread( routine, arg, cleanup );
}

void __sc_kill_thread( void *, va_list vl ) {
    auto id = va_arg( vl, __dios::ThreadId );
    auto reason = va_arg( vl, int );

    __dios::Scheduler().kill_thread( id, reason );
}

void __sc_get_thread_id( void *retval, va_list vl ) {
    auto ret = static_cast< __dios::ThreadId * >( retval );
    *ret = __dios::Scheduler().get_thread_id();
}
