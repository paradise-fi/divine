// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#ifndef __DIOS_FAULT_H__
#define __DIOS_FAULT_H__

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#endif

EXTERN_C
extern uint8_t const *_DiOS_fault_cfg;
CPP_END

#ifdef __cplusplus

#include <array>
#include <cstdarg>
#include <dios.h>
#include <dios/stdlibwrap.hpp>

namespace __dios {

struct Fault {
    Fault() : triggered( false ), ready( false ) {
        config.fill( FaultFlag::Enabled | FaultFlag::AllowOverride );
        ready = true;

        // Initialize pointer to C-compatible _DiOS_fault_cfg
        __dios_assert( !_DiOS_fault_cfg );
        _DiOS_fault_cfg = &config[ 0 ];
    }

    enum FaultFlag
    {
        Enabled       = 0x01,
        Continue      = 0x02,
        UserSpec      = 0x04,
        AllowOverride = 0x08,
    };

    static constexpr int fault_count = _DiOS_SF_Last;
    static _VM_Fault str_to_fault( dstring fault );
    bool load_user_pref( const _VM_Env *env );
    static void __attribute__((__noreturn__)) handler( _VM_Fault what, _VM_Frame *cont_frame, void (*cont_pc)(), ... ) noexcept;

    std::array< uint8_t, fault_count > config;
    bool triggered;
    bool ready;
};

} // namespace __dios

namespace __sc {

void configure_fault( __dios::Context& ctx, void* retval, va_list vl );
void get_fault_config( __dios::Context& ctx, void* retval, va_list vl );

} // namespace __sc

#endif // __cplusplus

#endif // __DIOS_FAULT_H__
