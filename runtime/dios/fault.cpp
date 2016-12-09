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
    int err;
    sc_handler( ctx, &err, ret, vl );
    va_end( vl );
}

void Fault::sc_handler( __dios::Context& ctx, int *, void *, va_list vl ) noexcept {
    bool kernel = va_arg( vl, int );
    auto *frame = va_arg( vl, _VM_Frame * );
    auto what = va_arg( vl, int );

    InTrace _; // avoid dumping what we do

    if ( what >= fault_count ) {
        traceInternal( 0, "Unknown fault in handler" );
        die( ctx );
    }

    uint8_t fault_cfg = ctx.fault->config[ what ];
    if ( !ctx.fault->ready || fault_cfg & FaultFlag::Enabled ) {
        if ( kernel )
            traceInternal( 0, "Fault in kernel: %s", fault_to_str( what ).c_str() );
        else
            traceInternal( 0, "Fault in userspace: %s", fault_to_str( what ).c_str() );
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
        sc_handler_wrap( *ctx, nullptr, kernel, frame, _what );
    }
    else {
        __dios_syscall( _SC_fault_handler, nullptr, kernel, frame, _what  );
    }

    // Continue if we get the control back
    __vm_control( _VM_CA_Set, _VM_CR_Frame, cont_frame,
                  _VM_CA_Set, _VM_CR_PC, cont_pc,
                  _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, mask ? _VM_CF_Mask : 0 );
    __builtin_unreachable();
}

int Fault::str_to_fault( String fault ) {
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
    if ( fault == "diosconfig" )
        return static_cast< _VM_Fault >( _DiOS_F_Config );
    if ( fault == "malloc" )
        return static_cast< _VM_Fault >( _DiOS_SF_Malloc );
    return static_cast< _VM_Fault >( -1 );
}

String Fault::fault_to_str( int f ) {
    switch( f ) {
    case _VM_F_Assert: return "assert";
    case _VM_F_Arithmetic: return "arithmetic";
    case _VM_F_Memory: return "memory";
    case _VM_F_Control: return "control";
    case _VM_F_Locking: return "locking";
    case _VM_F_Hypercall: return "hypercall";
    case _VM_F_NotImplemented: return "notimplemented";
    case _DiOS_F_Assert: return "diosassert";
    case _DiOS_F_Config: return "diosconfig";
    case _DiOS_SF_Malloc: return "malloc";
    }
    return "unknown";
}

void Fault::trace_config( int indent ) {
    __dios_trace_i( indent, "fault and simfail configuration:" );
    for (int f = _VM_F_NoFault + 1; f != _DiOS_SF_Last; f++ ) {
        uint8_t cfg = config[f];
        String name = fault_to_str( f );
        String force = cfg & FaultFlag::AllowOverride ? "" : "force-";
        String state;
        if ( f >= _DiOS_F_Last )
            state = cfg & FaultFlag::Enabled ? "simfail" : "nofail";
        else {
            if ( cfg & FaultFlag::Enabled && cfg & FaultFlag::Continue )
                state = "report";
            else if ( cfg & FaultFlag::Enabled )
                state = "abort";
            else
                state = "ignore";
        }
        String def = cfg & FaultFlag::UserSpec ? "user" : "default";

        __dios_trace_i( indent + 1, "%s: %s%s, %s", name.c_str(), force.c_str(),
            state.c_str(), def.c_str() );;
    }
}

bool Fault::load_user_pref( const SysOpts& opts ) {
    for( const auto& i : opts ) {
        String cmd = i.first;
        String f = i.second;

        uint8_t cfg = FaultFlag::AllowOverride | FaultFlag::UserSpec;
        const String force("force-");
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
        else continue;

        int f_num = str_to_fault( f );
        if ( f_num == - 1 ) {
            __dios_trace( 0, "Invalid argument '%s' in option '%s:%s'",
                f.c_str(), cmd.c_str(), f.c_str() );
            return false;
        }

        if ( simfail && f_num < _DiOS_F_Last ) {
            __dios_trace( 0, "Invalid argument '%s' for command '%s'",
                f.c_str(), cmd.c_str() );
            return false;
        }

        config[ f_num ] = cfg;
    }
    return true;
}

} // namespace __dios

namespace __sc {

int internalFaultToStatus( __dios::Context& ctx, int fault ) {
    using FaultFlag = __dios::Fault::FaultFlag;
    uint8_t cfg = ctx.fault->config[ fault ];
    if ( fault < _DiOS_F_Last) { // Fault
        switch ( cfg & (FaultFlag::Enabled | FaultFlag::Continue) ) {
            case 0:
                return _DiOS_FC_Ignore;
            case FaultFlag::Enabled:
                return _DiOS_FC_Abort;
            case FaultFlag::Enabled | FaultFlag::Continue:
                return _DiOS_FC_Report;
            default:
                __builtin_unreachable();
        }
    }
    else { // Simfail
        if ( cfg | FaultFlag::Enabled )
            return _DiOS_FC_SimFail;
        else
            return _DiOS_FC_NoFail;
    }
}

void configure_fault( __dios::Context& ctx, int *, void *retval, va_list vl ) {
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

    *res = internalFaultToStatus( ctx, fault );
    int cfg = va_arg( vl, int );
    if ( fault < _DiOS_F_Last) { // Fault
        switch( cfg ) {
        case _DiOS_FC_Ignore:
            fc = FaultFlag::AllowOverride;
            break;
        case _DiOS_FC_Report:
            fc = FaultFlag::AllowOverride | FaultFlag::Enabled | FaultFlag::Continue;
            break;
        case _DiOS_FC_Abort:
            fc = FaultFlag::AllowOverride | FaultFlag::Enabled;
            break;
        default:
            *res = _DiOS_FC_EInvalidCfg;
        }
    }
    else { // Simfail
        switch( cfg ) {
        case _DiOS_FC_SimFail:
            fc = FaultFlag::AllowOverride | FaultFlag::Enabled;
            break;
        case _DiOS_FC_NoFail:
            fc = FaultFlag::AllowOverride;
            break;
        default:
            *res = _DiOS_FC_EInvalidCfg;
        }
    }
}

void get_fault_config( __dios::Context& ctx, int *, void* retval, va_list vl ) {
    auto fault = va_arg( vl, int );
    auto res = static_cast< int * >( retval );
    if ( fault >= __dios::Fault::fault_count ) {
        *res = _DiOS_FC_EInvalidFault;
        return;
    }

    *res = internalFaultToStatus( ctx, fault );
}

void fault_handler( __dios::Context& ctx, int *err, void* retval, va_list vl ) {
     __dios::Fault::sc_handler(ctx, err, retval, vl);
}

} // namespace __sc

int __dios_configure_fault( int fault, int cfg ) {
    int ret;
    __dios_syscall( __dios::_SC_configure_fault, &ret, fault, cfg );
    return ret;
}

int __dios_get_fault_config( int fault ) {
    int ret;
    __dios_syscall( __dios::_SC_get_fault_config, &ret, fault );
    return ret;
}

void __dios_fault( int f, const char *msg, ... ) {
    __dios_trace_t( msg );
    auto fh = reinterpret_cast< __vm_fault_t >( __vm_control( _VM_CA_Get, _VM_CR_FaultHandler ) );
    auto *retFrame = static_cast< struct _VM_Frame * >(
        __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent;
    auto pc = reinterpret_cast< uintptr_t >( retFrame->pc );

    typedef void (*PC)(void);
    ( *fh )( static_cast< _VM_Fault >( f ), retFrame, reinterpret_cast< PC >( pc ) );
}
