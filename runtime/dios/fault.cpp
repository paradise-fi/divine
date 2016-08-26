// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>


#include <dios/fault.hpp>
#include <dios/trace.hpp>
#include <dios/syscall.hpp>

namespace __dios {

Fault *Fault::_inst;

void __attribute__((__noreturn__)) Fault::handler( _VM_Fault _what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept
{
    auto what = static_cast< int >( _what );
    auto mask = __vm_mask( 1 );
    InTrace _; // avoid dumping what we do

    __dios_assert_v( what < _DiOS_Fault::_DiOS_F_Last, "Unknown falt" );
    int fault = _inst->config[ what ];
    if ( fault & _DiOS_FF_Enabled ) {
        __dios_trace_t( "VM Fault" );
        __vm_trace( _VM_T_Flag, what );
        __dios_trace_t( "Backtrace:" );
        int i = 0;
        for ( auto *f = __vm_query_frame()->parent; f != nullptr; f = f->parent )
            traceInternal( 0, "%d: %s", ++i, __md_get_pc_meta( uintptr_t( f->pc ) )->name );

        if ( !( fault & _DiOS_FF_Continue ) ) {
            _inst->triggered = true;
            __dios_syscall_trap();
        }
    }

    // Continue
    __vm_jump( cont_frame, cont_pc, !mask );
    __builtin_unreachable();
}

void Fault::load_user_pref( const _VM_Env *env ) {
    __dios_trace_t( "Warning: Loading user fault preferences is not implemented" );
}

} // namespace __dios

namespace __sc {

void configure_fault( __dios::Context& ctx, void* retval, va_list vl ) {
    auto fault = va_arg( vl, int );
    auto res = static_cast< int * >( retval );
    if ( fault >= _DiOS_Fault::_DiOS_F_Last ) {
        *res = _DiOS_FaultFlag::_DiOS_FF_InvalidFault;
        return;
    }

    int& fc = ctx.fault->config[ fault ];
    *res = fc;

    if ( !( fc & _DiOS_FaultFlag::_DiOS_FF_AllowOverride ) )
        return;


    int en = va_arg( vl, int );
    if ( en == 0 )
        fc &= ~_DiOS_FaultFlag::_DiOS_FF_Enabled;
    else if ( en > 0)
        fc |= _DiOS_FaultFlag::_DiOS_FF_Enabled;

    int cont = va_arg( vl, int );
    if ( cont == 0 )
        fc &= ~_DiOS_FaultFlag::_DiOS_FF_Continue;
    else if ( cont > 0 )
        fc |= _DiOS_FaultFlag::_DiOS_FF_Continue;

    return;
}

void get_fault_config( __dios::Context& ctx, void* retval, va_list vl ) {
    auto fault = va_arg( vl, int );
    auto res = static_cast< int * >( retval );
    if ( fault >= _DiOS_Fault::_DiOS_F_Last )
        *res = _DiOS_FaultFlag::_DiOS_FF_InvalidFault;
    else
        *res = ctx.fault->config[ fault ];
}

} // namespace __sc

int __dios_configure_fault( int fault, int enable, int cont ) {
    int ret;
    __dios_syscall( __dios::_SC_CONFIGURE_FAULT, &ret, fault, enable, cont );
    return ret;
}

int __dios_get_fault_config( int fault ) {
    int ret;
    __dios_syscall( __dios::_SC_GET_FAULT_CONFIG, &ret, fault );
    return ret;
}
