// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#ifndef __DIOS_FAULT_H__
#define __DIOS_FAULT_H__

#include <array>
#include <cstdarg>
#include <dios.h>

namespace __dios {

struct Fault {
    std::array< int, _DiOS_Fault::_DiOS_F_Last > config;
    bool triggered;
    bool ready;

    Fault() : triggered( false ), ready( false ) {
        config.fill( _DiOS_FaultFlag::_DiOS_FF_Enabled |
                     _DiOS_FaultFlag::_DiOS_FF_AllowOverride );
        ready = true;
    }

    void load_user_pref( const _VM_Env *env );
    static void __attribute__((__noreturn__)) handler( _VM_Fault what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept;
};

} // namespace __dios

namespace __sc {

void configure_fault( __dios::Context& ctx, void* retval, va_list vl );
void get_fault_config( __dios::Context& ctx, void* retval, va_list vl );

} // namespace __sc


#endif // __DIOS_FAULT_H__
