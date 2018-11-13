// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <cstdint>
#include <dios/sys/fault.hpp>

uint8_t __dios_simfail_flags;

namespace __dios
{

    void FaultBase::updateSimFail( uint8_t *config )
    {
        config = config ? config : this->config();
        uint8_t flags = 0;
        uint8_t bit = 1;
        for ( int x = _DiOS_SF_First; x != _DiOS_SF_Last; x++, bit <<= 1 )
            flags |= config[ x ] & FaultFlag::Enabled ? bit : 0;
        auto *meta = __md_get_global_meta( "__dios_simfail_flags" );
        if ( !meta )
            __dios_trace_t( "Warning: Cannot update SimFail" );
        else
            *reinterpret_cast< char *>( meta->address ) = flags;
    }

    void FaultBase::backtrace( _VM_Frame * frame )
    {
        if ( ( _flags & PrintBacktrace ) == 0 )
            return;
        __dios_trace_t( "Backtrace:" );
        auto guard = makeMallocNofail();
        char *buffer = static_cast< char * >( malloc( 1024 ) );
        size_t len = 1024;
        int i = 0;
        for ( auto *f = frame; f != nullptr; f = f->parent ) {
            const char *name = __md_get_pc_meta( f->pc )->name;
            int status;
            char *b = __cxxabiv1::__cxa_demangle( name, buffer, &len, &status );
            if ( b )
                __dios_trace_f( "  %d: %s", ++i, b );
            else
                __dios_trace_f( "  %d: %s", ++i, name );
        }
        free( buffer );
    }

    void FaultBase::fault_handler( int kernel, _VM_Frame * frame, int what )
    {
        if ( what >= fault_count )
        {
            __dios_trace_t( "Unknown fault in handler" );
            die();
        }

        auto *cfg = config();
        // If no task exists, trigger the fault
        uint8_t fault_cfg = cfg ? cfg[ what ] : FaultFlag::Enabled;

        if ( fault_cfg & FaultFlag::Detect )
            __vm_ctl_flag( 0, _DiOS_CF_Fault );

        if ( !( _flags & Ready ) || fault_cfg & FaultFlag::Enabled )
        {
            __dios_trace_f( "FATAL: %s in %s", fault_to_str( what, true ).c_str(),
                            kernel ? "kernel" : "userspace" );
            __vm_ctl_flag( 0, _VM_CF_Error );
            backtrace( frame );

            if ( !( _flags & Ready ) || !( fault_cfg & FaultFlag::Continue ) )
                die();
        }
    }

    int FaultBase::str_to_fault( String fault )
    {
        std::transform( fault.begin(), fault.end(), fault.begin(), ::tolower );
        if ( fault == "assert" )
            return _VM_F_Assert;
        if ( fault == "integer" )
            return _VM_F_Integer;
        if ( fault == "float" )
            return _VM_F_Float;
        if ( fault == "memory" )
            return _VM_F_Memory;
        if ( fault == "leak" )
            return _VM_F_Leak;
        if ( fault == "control" )
            return _VM_F_Control;
        if ( fault == "locking" )
            return _VM_F_Locking;
        if ( fault == "hypercall" )
            return _VM_F_Hypercall;
        if ( fault == "notimplemented" )
            return _VM_F_NotImplemented;
        if ( fault == "exit" )
            return static_cast< _VM_Fault >( _DiOS_F_Exit );
        if ( fault == "diosassert" )
            return static_cast< _VM_Fault >( _DiOS_F_Assert );
        if ( fault == "diosconfig" )
            return static_cast< _VM_Fault >( _DiOS_F_Config );
        if ( fault == "malloc" )
            return static_cast< _VM_Fault >( _DiOS_SF_Malloc );
        return static_cast< _VM_Fault >( -1 );
    }

    String FaultBase::fault_to_str( int f, bool ext )
    {
        switch( f )
        {
            case _VM_F_Assert: return ext ? "assertion failure" : "assert";
            case _VM_F_Integer: return ext ? "integer arithmetic error" : "integer";
            case _VM_F_Float: return ext ? "floating-point arithmetic error" : "float";
            case _VM_F_Memory: return ext ? "memory error" : "memory";
            case _VM_F_Leak: return ext ? "memory leak" : "leak";
            case _VM_F_Control: return ext ? "control error" : "control";
            case _VM_F_Locking: return ext ? "locking error" : "locking";
            case _VM_F_Hypercall: return ext ? "bad hypercall" : "hypercall";
            case _VM_F_NotImplemented: return ext ? "not implemented" : "notimplemented";
            case _DiOS_F_Assert: return ext ? "dios assertion violation" : "diosassert";
            case _DiOS_F_Config: return ext ? "config error" : "diosconfig";
            case _DiOS_SF_Malloc: return "malloc";
        }
        return "unknown";
    }

    void FaultBase::trace_config( int indent )
    {
        auto config = this->config();
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

    void FaultBase::load_user_pref( uint8_t *config, SysOpts& opts )
    {
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
        updateSimFail( config );
    }

    int FaultBase::_faultToStatus( int fault )
    {
        auto config = this->config();
        uint8_t cfg = config[ fault ];
        if ( fault < _DiOS_F_Last) // Fault
        {
            switch ( cfg & ( FaultFlag::Enabled | FaultFlag::Continue | FaultFlag::Detect ) )
            {
                case 0:
                    return _DiOS_FC_Ignore;
                case FaultFlag::Enabled:
                    return _DiOS_FC_Abort;
                case FaultFlag::Enabled | FaultFlag::Continue:
                    return _DiOS_FC_Report;
                case FaultFlag::Detect:
                    return _DiOS_FC_Detect;
                default:
                    __builtin_unreachable();
            }
        }
        else // Simfail
        {
            if ( cfg | FaultFlag::Enabled )
                return _DiOS_FC_SimFail;
            else
                return _DiOS_FC_NoFail;
        }
    }

    int FaultBase::configure_fault( int fault, int cfg )
    {
        auto config = this->config();
        if ( fault >= fault_count )
            return _DiOS_FC_EInvalidFault;

        uint8_t& fc = config[ fault ];
        if ( !( fc & FaultFlag::AllowOverride ) )
            return _DiOS_FC_ELocked;

        int lastCfg = _faultToStatus( fault );
        if ( fault < _DiOS_F_Last ) // Fault
        {
            switch( cfg )
            {
                case _DiOS_FC_Ignore:
                    fc = FaultFlag::AllowOverride;
                    break;
                case _DiOS_FC_Detect:
                    fc = FaultFlag::AllowOverride | FaultFlag::Detect;
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
        else // Simfail
        {
            switch( cfg )
            {
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
        updateSimFail();
        return lastCfg;
    }

    int FaultBase::get_fault_config( int fault )
    {
        if ( fault >= fault_count )
            return _DiOS_FC_EInvalidFault;
        return _faultToStatus( fault );
    }
}
