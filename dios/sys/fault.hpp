// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#ifndef __DIOS_FAULT_H__
#define __DIOS_FAULT_H__

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#endif

#ifdef __cplusplus

#include <algorithm>
#include <cstdarg>
#include <cxxabi.h>
#include <sys/metadata.h>
#include <sys/trace.h>

#include <dios/sys/main.hpp>
#include <dios/sys/trace.hpp>
#include <dios/sys/syscall.hpp>

namespace __dios {

enum FaultFlag
{
    Enabled       = 0x01,
    Continue      = 0x02,
    UserSpec      = 0x04,
    AllowOverride = 0x08,
    Artificial    = 0x10,
    Detect        = 0x20
};

template < typename Next >
struct Fault: public Next {
    static constexpr int fault_count = _DiOS_SF_Last;

    struct Process : Next::Process
    {
        Process()
        {
            std::fill_n( faultConfig, _DiOS_SF_Last, FaultFlag::Enabled | FaultFlag::AllowOverride );
        }

        uint8_t faultConfig[ _DiOS_SF_Last ];
    };

    uint8_t _flags;
    enum Flags { Ready = 1, PrintBacktrace = 2 };

    struct MallocNofailGuard {
    private:
        MallocNofailGuard( Fault< Next >& f )
            : _f( f ),
              _config( _f.getCurrentConfig() ),
              _s( _config[ _DiOS_SF_Malloc ] )
        {
            _config[ _DiOS_SF_Malloc ] = 0;
            _f.updateSimFail();
        }
    public:
        ~MallocNofailGuard() {
            _config[ _DiOS_SF_Malloc ] = _s;
            _f.updateSimFail();
        }

        Fault& _f;
        uint8_t *_config;
        uint8_t _s;

        friend Fault;
    };

    MallocNofailGuard makeMallocNofail() {
        return { *this };
    }

    Fault() : _flags( 0 ) {}

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< Fault >( "{Fault}" );
        __vm_ctl_set( _VM_CR_FaultHandler,
                      reinterpret_cast< void * >( handler< typename Setup::Context > ) );
        load_user_pref( s.proc1->faultConfig, s.opts );

        if ( extractOpt( "debug", "faultcfg", s.opts ) )
        {
            trace_config( 1 );
            __vm_ctl_flag( 0, _VM_CF_Error );
        }
        _flags = Ready;
        if ( extractOpt( "debug", "faultbt", s.opts ) )
            _flags |= PrintBacktrace;
        Next::setup( s );
    }

    void getHelp( Map< String, HelpOption >& options ) {
        const char *opt1 = "[force-]{ignore|report|abort}";
        if ( options.find( opt1 ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt1 );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        }
        options[ opt1 ] = { "configure the fault handler",
                            { "assert", "arithmetic", "memory", "control",
                              "locking", "hypercall", "notimplemented", "diosassert" } };

        const char *opt2 = "{nofail|simfail}";
        if ( options.find( opt2 ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt2 );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        }
        options[ opt2 ] = { "enable/disable simulation of failure",
          { "malloc" } };

        Next::getHelp( options );
    }

    uint8_t *getCurrentConfig() {
        auto *task = Next::getCurrentTask();
        if ( !task )
            return nullptr;
        return static_cast< Process * >( task->_proc )->faultConfig;
    }

    void updateSimFail( uint8_t *config = nullptr )
    {
        config = config ? config : getCurrentConfig();
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

    void backtrace( _VM_Frame * frame )
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

    void fault_handler( int kernel, _VM_Frame * frame, int what )
    {
        if ( what >= fault_count )
        {
            __dios_trace_t( "Unknown fault in handler" );
            Next::die();
        }

        auto *cfg = getCurrentConfig();
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
                Next::die();
        }
    }

    template < typename Context >
    static __trapfn void handler( _VM_Fault _what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept
    {
        uint64_t old = __vm_ctl_flag( 0, _VM_CF_KernelMode | _VM_CF_IgnoreCrit | _VM_CF_IgnoreLoop );

        if ( old & _DiOS_CF_IgnoreFault )
        {
            __vm_ctl_set( _VM_CR_Flags, reinterpret_cast< void * >( old & ~_DiOS_CF_IgnoreFault ) );
            __vm_ctl_set( _VM_CR_Frame, cont_frame, cont_pc );
        }

        __dios_sync_parent_frame();

        bool kernel = old & _VM_CF_KernelMode;
        auto *frame = __dios_this_frame()->parent;
        auto& fault = get_state< Context >();
        fault.fault_handler( kernel, frame, _what );

        // Continue if we get the control back
        old |= uint64_t( __vm_ctl_get( _VM_CR_Flags ) ) & ( _DiOS_CF_Fault | _VM_CF_Error );
        __vm_ctl_set( _VM_CR_Flags, reinterpret_cast< void * >( old ) );
        __vm_ctl_set( _VM_CR_Frame, cont_frame, cont_pc );
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

    static String fault_to_str( int f, bool ext = false )  {
        switch( f )
        {
            case _VM_F_Assert: return ext ? "assertion failure" : "assert";
            case _VM_F_Arithmetic: return ext ? "arithmetic error" : "arithmetic";
            case _VM_F_Memory: return ext ? "memory error" : "memory";
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

    void trace_config( int indent ) {
        auto config = getCurrentConfig();
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

    void load_user_pref( uint8_t *config, SysOpts& opts )  {
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

    int _faultToStatus( int fault )
    {
        auto config = getCurrentConfig();
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

    int configure_fault( int fault, int cfg )
    {
        auto config = getCurrentConfig();
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

    int get_fault_config( int fault )
    {
        if ( fault >= fault_count )
            return _DiOS_FC_EInvalidFault;
        return _faultToStatus( fault );
    }
};

} // namespace __dios

#endif // __cplusplus

#endif // __DIOS_FAULT_H__
