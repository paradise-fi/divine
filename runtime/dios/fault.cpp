// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <algorithm>
#include <cctype>

#include <divine/metadata.h>
#include <dios/scheduling.hpp>
#include <dios/fault.hpp>
#include <dios/trace.hpp>
#include <dios/syscall.hpp>

uint8_t const *_DiOS_fault_cfg;

namespace __dios {

void Fault::die( __dios::Context& ctx ) noexcept {
    ctx.scheduler->killProcess( nullptr );
    static_cast< _VM_Frame * >
        ( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent = nullptr;
}

void Fault::sc_handler_wrap( __dios::Context& ctx, void *ret, ... ) noexcept {
    va_list vl;
    va_start( vl, ret );
    sc_handler( ctx, ret, vl );
    va_end( vl );
}

void Fault::sc_handler( __dios::Context& ctx, void *retval, va_list vl ) noexcept {
    typedef void (*PC)();
    bool kernel = va_arg( vl, int );
    auto *frame = va_arg( vl, _VM_Frame * );
    auto what = va_arg( vl, int );
    auto *cont_frame = va_arg( vl, _VM_Frame * );
    auto cont_pc = va_arg( vl, PC );

    InTrace _; // avoid dumping what we do

    if ( what >= fault_count ) {
        traceInternal( 0, "Unknown fault in handler" );
        die( ctx );
    }

    uint8_t fault_cfg = ctx.fault->config[ what ];
    if ( !ctx.fault->ready || fault_cfg & FaultFlag::Enabled ) {
        if ( kernel )
            traceInternal( 0, "Kernel VM Fault" );
        else
            traceInternal( 0, "Userspace VM Fault" );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        traceInternal( 0, "Backtrace:" );
        int i = 0;
        for ( auto *f = frame; f != nullptr; f = f->parent ) {
            traceInternal( 0, "  %d: %s", ++i, __md_get_pc_meta( uintptr_t( f->pc ) )->name );
        }

        if ( !ctx.fault->ready || !( fault_cfg & FaultFlag::Continue ) )
            die( ctx );
    }
}

void Fault::handler( _VM_Fault _what, _VM_Frame *cont_frame,
                                                   void (*cont_pc)(), ... ) noexcept
{
    auto mask = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
    int kernel = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags ) ) & _VM_CF_KernelMode;
    auto *frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent;

    if ( kernel ) {
        auto *ctx = static_cast< Context * >( __vm_control( _VM_CA_Get, _VM_CR_State ) );
        sc_handler_wrap( *ctx, nullptr, kernel, frame, _what, cont_frame, cont_pc );
    }
    else {
        __dios_syscall( _SC_FAULT_HANDLER, nullptr, kernel, frame, _what, cont_frame, cont_pc  );
    }

    // Continue if we get the control back
    __vm_control( _VM_CA_Set, _VM_CR_Frame, cont_frame,
                  _VM_CA_Set, _VM_CR_PC, cont_pc,
                  _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, mask ? _VM_CF_Mask : 0 );
    __builtin_unreachable();
}

_VM_Fault Fault::str_to_fault( dstring fault ) {
    std::transform( fault.begin(), fault.end(), fault.begin(), ::tolower );
    if ( fault == "assert" )
        return _VM_F_Assert;
    if ( fault == "arithmetic" )
        return _VM_F_Arithmetic;
    if ( fault == "memory" )
        return _VM_F_Memory;
    if ( fault == "control" )
        return _VM_F_Control;
    if ( fault == "locking" )
        return _VM_F_Locking;
    if ( fault == "hypercall" )
        return _VM_F_Hypercall;
    if ( fault == "notimplemented" )
        return _VM_F_NotImplemented;
    if ( fault == "diosassert" )
        return static_cast< _VM_Fault >( _DiOS_F_Assert );
    if ( fault == "malloc" )
        return static_cast< _VM_Fault >( _DiOS_SF_Malloc );
    return static_cast< _VM_Fault >( -1 );
}

bool Fault::load_user_pref( const _VM_Env *e ) {
    const char *prefix = "sys";
    int pref_len = strlen( prefix );
    for( ; e->key != nullptr; e++ ) {
        if ( memcmp( prefix, e->key, pref_len ) != 0 )
            continue;
        dstring s( e->value, e->size );
        auto p = std::find( s.begin(), s.end(), ':' );
        if ( p == s.end() ) {
            __dios_trace( 0, "Missing ':' in parameter '%.*s'", e->size, e->value );
            return false;
        }

        if ( std::find( ++p, s.end(), ':') != s.end() ) {
            __dios_trace( 0, "Multiple ':' in parameter '%.*s'", e->size, e->value );
            return false;
        }

        dstring f( p, s.end() );
        dstring cmd( s.begin(), --p );

        uint8_t cfg = FaultFlag::AllowOverride | FaultFlag::UserSpec;
        const dstring force("force-");
        if ( cmd.size() > force.size() && std::equal( force.begin(), force.end(), cmd.begin() ) ) {
            cfg &= ~FaultFlag::AllowOverride;
            cmd = cmd.substr( force.size() );
        }

        bool simfail = false;
        if      ( cmd == "ignore" )  { }
        else if ( cmd == "report" )  { cfg |= FaultFlag::Enabled | FaultFlag::Continue; }
        else if ( cmd == "abort" )   { cfg |= FaultFlag::Enabled; }
        else if ( cmd == "nofail" )  { simfail = true; }
        else if ( cmd == "simfail" ) { cfg |= FaultFlag::Enabled; simfail = true; }
        else {
            __dios_trace( 0, "Unknow command '%s' in in parameter '%.*s'",
                cmd.c_str(), e->size, e->value );
            return false;
        }

        int f_num = str_to_fault( f );
        if ( f_num == - 1 ) {
            __dios_trace( 0, "Invalid argument '%s' in in parameter '%.*s'",
                f.c_str(), e->size, e->value );
            return false;
        }

        if ( simfail && f_num < _DiOS_F_Last ) {
            __dios_trace( 0, "Invalid argument '%s' for command '%s' in parameter '%.*s'",
                f.c_str(), cmd.c_str(), e->size, e->value );
            return false;
        }

        config[ f_num ] = cfg;
    }
    return true;
}

} // namespace __dios

namespace __sc {

void configure_fault( __dios::Context& ctx, void* retval, va_list vl ) {
    using FaultFlag = __dios::Fault::FaultFlag;
    auto fault = va_arg( vl, int );
    auto res = static_cast< int * >( retval );
    if ( fault >= __dios::Fault::fault_count ) {
        *res = _DiOS_FC_EInvalidFault;
        return;
    }

    uint8_t& fc = ctx.fault->config[ fault ];
    if ( !( fc & FaultFlag::AllowOverride ) ) {
        *res = _DiOS_FC_ELocked;
        return;
    }

    *res = fc;
    int cfg = va_arg( vl, int );
    if ( fault < _DiOS_F_Last) { // Fault
        switch( cfg ) {
        case _DiOS_FC_Ignore:
            fc = 0;
            break;
        case _DiOS_FC_Report:
            fc = FaultFlag::Enabled | FaultFlag::Continue;
            break;
        case _DiOS_FC_Abort:
            fc = FaultFlag::Enabled;
            break;
        default:
            *res = _DiOS_FC_EInvalidCfg;
        }
    }
    else { // Simfail
        switch( cfg ) {
        case _DiOS_FC_SimFail:
            fc = FaultFlag::Enabled;
            break;
        case _DiOS_FC_NoFail:
            fc = 0;
            break;
        default:
            *res = _DiOS_FC_EInvalidCfg;
        }
    }
}

void get_fault_config( __dios::Context& ctx, void* retval, va_list vl ) {
    using FaultFlag = __dios::Fault::FaultFlag;
    auto fault = va_arg( vl, int );
    auto res = static_cast< int * >( retval );
    if ( fault >= __dios::Fault::fault_count ) {
        *res = _DiOS_FC_EInvalidFault;
        return;
    }

    uint8_t cfg = ctx.fault->config[ fault ];
    if ( fault < _DiOS_F_Last) { // Fault
        if ( !( cfg | FaultFlag::Enabled ) )
            *res = _DiOS_FC_Ignore;
        else if (  cfg | FaultFlag::Continue )
            *res = _DiOS_FC_Report;
        else
            *res = _DiOS_FC_Abort;
    }
    else { // Simfail
        if ( cfg | FaultFlag::Enabled )
            *res = _DiOS_FC_SimFail;
        else
            *res = _DiOS_FC_NoFail;
    }
}

} // namespace __sc

int __dios_configure_fault( int fault, int cfg ) {
    int ret;
    __dios_syscall( __dios::_SC_CONFIGURE_FAULT, &ret, fault, cfg );
    return ret;
}

int __dios_get_fault_config( int fault ) {
    int ret;
    __dios_syscall( __dios::_SC_GET_FAULT_CONFIG, &ret, fault );
    return ret;
}

void __dios_fault( enum _VM_Fault f, const char *msg, ... ) {
    __dios_trace_t( msg );
    auto fh = reinterpret_cast< __vm_fault_t >( __vm_control( _VM_CA_Get, _VM_CR_FaultHandler ) );
    auto *retFrame = static_cast< struct _VM_Frame * >(
        __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent;
    auto pc = reinterpret_cast< uintptr_t >( retFrame->pc );

    typedef void (*PC)(void);
    ( *fh )( f, retFrame, reinterpret_cast< PC >( pc + 1 ) );
}
