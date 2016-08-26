// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <dios/scheduling.h>

_DiOS_ThreadId __dios_start_thread( _DiOS_FunPtr routine, void *arg,
    _DiOS_FunPtr cleanup ) noexcept
{
    _DiOS_ThreadId ret;
    __dios_syscall( __dios::_SC_START_THREAD, &ret, routine, arg, cleanup );
    return ret;
}

_DiOS_ThreadId __dios_get_thread_id() noexcept {
    _DiOS_ThreadId ret;
    __dios_syscall( __dios::_SC_GET_THREAD_ID, &ret );
    return ret;
}

void __dios_kill_thread( _DiOS_ThreadId id, int reason ) noexcept {
    __dios_syscall( __dios::_SC_KILL_THREAD, nullptr, id, reason );
}

void __sc_start_thread( __dios::Context& ctx, void *retval, va_list vl ) {
    auto routine = va_arg( vl, __dios::FunPtr );
    auto arg = va_arg( vl, void * );
    auto cleanup = va_arg( vl, __dios::FunPtr );
    auto ret = static_cast< __dios::ThreadId * >( retval );

    *ret = ctx.scheduler->start_thread( routine, arg, cleanup );
}

void __sc_kill_thread( __dios::Context& ctx, void *, va_list vl ) {
    auto id = va_arg( vl, __dios::ThreadId );
    auto reason = va_arg( vl, int );

    ctx.scheduler->kill_thread( id, reason );
}

void __sc_get_thread_id( __dios::Context& ctx, void *retval, va_list vl ) {
    auto ret = static_cast< __dios::ThreadId * >( retval );
    *ret = ctx.scheduler->get_thread_id();
}
