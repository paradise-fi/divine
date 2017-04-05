// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <algorithm>
#include <cctype>

#include <divine/metadata.h>
#include <dios/core/scheduling.hpp>
#include <dios/core/fault.hpp>
#include <dios/core/trace.hpp>
#include <dios/core/syscall.hpp>

uint8_t const *_DiOS_fault_cfg;

void __dios::Fault::die( __dios::Context& ctx ) noexcept
{
    ctx.scheduler->killProcess( 1 );
    static_cast< _VM_Frame * >
        ( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent = nullptr;
}

namespace __sc {

void fault_handler( __dios::Context& ctx, int *err, void* retval, va_list vl ) {
     __dios::Fault::sc_handler(ctx, err, retval, vl);
}

void configure_fault( __dios::Context& ctx, int *err, void* retval, va_list vl ) {
     __dios::Fault::configure_fault(ctx, err, retval, vl);
}

void get_fault_config( __dios::Context& ctx, int *err, void* retval, va_list vl ) {
     __dios::Fault::get_fault_config(ctx, err, retval, vl);
}

} // namespace __sc

namespace __sc_passthru {

void configure_fault( __dios::Context& ctx, int * err, void* retval, va_list vl )  {
    __sc::configure_fault(ctx, err, retval, vl);
}

void get_fault_config( __dios::Context& ctx, int * err, void* retval, va_list vl ) {
    __sc::get_fault_config(ctx, err, retval, vl);
}

void fault_handler( __dios::Context& ctx, int *err, void* retval, va_list vl ) {
    __sc::fault_handler(ctx, err, retval, vl);
}

} // namespace __sc_passthru
