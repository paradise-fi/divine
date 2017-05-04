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

#include <algorithm>
#include <cstdarg>
#include <divine/metadata.h>
#include <dios.h>
#include <dios/core/main.hpp>
#include <dios/core/trace.hpp>
#include <dios/core/scheduling.hpp>

namespace __dios {

enum FaultFlag
{
    Enabled       = 0x01,
    Continue      = 0x02,
    UserSpec      = 0x04,
    AllowOverride = 0x08,
};

template < typename Next >
struct Fault: public Next {
    static constexpr int fault_count = _DiOS_SF_Last;

    Fault() :
        config( static_cast< uint8_t *>( __vm_obj_make( fault_count ) ) ),
        triggered( false ),
        ready( false )
    {
        std::fill_n( config, fault_count, FaultFlag::Enabled | FaultFlag::AllowOverride );
        ready = true;

        // Initialize pointer to C-compatible _DiOS_fault_cfg
        __dios_assert( !_DiOS_fault_cfg );
        _DiOS_fault_cfg = config;
    }

    void setup( MemoryPool& pool, const _VM_Env *env, SysOpts opts ) {
        __vm_control( _VM_CA_Set, _VM_CR_FaultHandler, handler );
        load_user_pref( opts );

        if ( extractOpt( "debug", "faultcfg", opts ) ) {
            trace_config( 1 );
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        }

        Next::setup( pool, env, opts );
    }

    void getHelp( Map< String, HelpOption >& options ) {
        const char *opt1 = "[force-]{ignore|report|abort}";
        if ( options.find( opt1 ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt1 );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        }
        options[ opt1 ] = { "configure fault, force disables program override",
          { "assert",
            "arithmetic",
            "memory",
            "control",
            "locking",
            "hypercall",
            "notimplemented",
            "diosassert" } };

        const char *opt2 = "{nofail|simfail}";
        if ( options.find( opt2 ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt2 );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        }
        options[ opt2 ] = { "enable/disable simulation of failure",
          { "malloc" } };

        Next::getHelp( options );
    }

    void terminate() noexcept  {
        BaseContext::kernelSyscall( SYS_die, nullptr );
        static_cast< _VM_Frame * >
            ( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent = nullptr;
    }

    void fault_handler( int kernel, _VM_Frame * frame, int what )  {
        InTrace _; // avoid dumping what we do

        if ( what >= fault_count ) {
            traceInternal( 0, "Unknown fault in handler" );
            terminate();
        }

        uint8_t fault_cfg = config[ what ];
        if ( !ready || fault_cfg & FaultFlag::Enabled ) {
            if ( kernel )
                traceInternal( 0, "Fault in kernel: %s", fault_to_str( what ).c_str() );
            else
                traceInternal( 0, "Fault in userspace: %s", fault_to_str( what ).c_str() );
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
            traceInternal( 0, "Backtrace:" );
            int i = 0;
            for ( auto *f = frame; f != nullptr; f = f->parent ) {
                traceInternal( 0, "  %d: %s", ++i, __md_get_pc_meta( f->pc )->name );
            }

            if ( !ready || !( fault_cfg & FaultFlag::Continue ) )
                terminate();
        }
    }

    static void handler( _VM_Fault _what, _VM_Frame *cont_frame,
                         void (*cont_pc)(), ... ) noexcept
    {
        auto mask = reinterpret_cast< uintptr_t >(
            __vm_control( _VM_CA_Get, _VM_CR_Flags,
                          _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
        int kernel = reinterpret_cast< uintptr_t >(
            __vm_control( _VM_CA_Get, _VM_CR_Flags ) ) & _VM_CF_KernelMode;
        auto *frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent;

        if ( kernel ) {
            BaseContext::kernelSyscall( __vm_control( _VM_CA_Get, _VM_CR_State ),
                SYS_fault_handler, nullptr, kernel, frame, _what );
        }
        else {
            __dios_syscall( SYS_fault_handler, nullptr, kernel, frame, _what  );
        }

        // Continue if we get the control back
        __vm_control( _VM_CA_Set, _VM_CR_Frame, cont_frame,
                      _VM_CA_Set, _VM_CR_PC, cont_pc,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, mask ? _VM_CF_Mask : 0 );
        __builtin_unreachable();
    }

    static int str_to_fault( String fault ) {
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

    static String fault_to_str( int f )  {
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

    void trace_config( int indent ) {
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

    void load_user_pref( SysOpts& opts )  {
        SysOpts unused;
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
            else {
                unused.push_back( i );
                continue;
            };

            int f_num = str_to_fault( f );
            if ( f_num == - 1 ) {
                __dios_trace( 0, "Invalid argument '%s' in option '%s:%s'",
                    f.c_str(), cmd.c_str(), f.c_str() );
                __dios_fault( _DiOS_F_Config, "Invalid configuration of fault handler" );
            }

            if ( simfail && f_num < _DiOS_F_Last ) {
                __dios_trace( 0, "Invalid argument '%s' for command '%s'",
                    f.c_str(), cmd.c_str() );
                __dios_fault( _DiOS_F_Config, "Invalid configuration of fault handler" );
            }

            config[ f_num ] = cfg;
        }
        opts = unused;
    }

    int _faultToStatus( int fault )  {
        uint8_t cfg = config[ fault ];
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

    int configure_fault( int fault, int cfg ) {
        if ( fault >= fault_count ) {
            return _DiOS_FC_EInvalidFault;
        }

        uint8_t& fc = config[ fault ];
        if ( !( fc & FaultFlag::AllowOverride ) ) {
            return _DiOS_FC_ELocked;
        }

        int lastCfg = _faultToStatus( fault );
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
                    return _DiOS_FC_EInvalidCfg;
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
                    return _DiOS_FC_EInvalidCfg;
            }
        }
        return lastCfg;
    }

    int get_fault_config( int fault ) {
        if ( fault >= fault_count )
            return _DiOS_FC_EInvalidFault;
        return _faultToStatus( fault );
    }

    uint8_t *config;
    bool triggered;
    bool ready;
};

} // namespace __dios

#endif // __cplusplus

#endif // __DIOS_FAULT_H__
