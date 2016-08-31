// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>


#include <dios/fault.hpp>
#include <dios/trace.hpp>
#include <dios/syscall.hpp>

namespace __dios {

Fault *Fault::_inst;

void __attribute__((__noreturn__)) Fault::handler( _VM_Fault _what, _VM_Frame *cont_frame,
                                                   void (*cont_pc)() ) noexcept
{
    auto what = static_cast< int >( _what );
    auto mask = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
    InTrace _; // avoid dumping what we do

    __dios_assert_v( what < _DiOS_Fault::_DiOS_F_Last, "Unknown falt" );
    int fault = _inst->config[ what ];
    if ( fault & _DiOS_FF_Enabled ) {
        __dios_trace_t( "VM Fault" );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        __dios_trace_t( "Backtrace:" );
        int i = 0;
        for ( auto *f = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent;
              f != nullptr; f = f->parent )
            traceInternal( 0, "%d: %s", ++i, __md_get_pc_meta( uintptr_t( f->pc ) )->name );

        if ( !( fault & _DiOS_FF_Continue ) ) {
            _inst->triggered = true;
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
        }
    }

    // Continue
    __vm_control( _VM_CA_Set, _VM_CR_Frame, cont_frame,
                  _VM_CA_Set, _VM_CR_PC, cont_pc,
                  _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, mask ? _VM_CF_Mask : 0 );
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
