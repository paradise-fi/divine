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
#include <sys/metadata.h>
#include <sys/trace.h>

#include <dios/sys/main.hpp>
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

struct ProcessFaultInfo
{
    ProcessFaultInfo()
    {
        std::fill_n( faultConfig, _DiOS_SF_Last, FaultFlag::Enabled | FaultFlag::AllowOverride );
    }

    uint8_t faultConfig[ _DiOS_SF_Last ];
};

struct FaultBase
{
    virtual uint8_t *config() = 0;
    virtual void die() = 0;

    uint8_t _flags;
    enum Flags { Ready = 1, PrintBacktrace = 2 };
    static constexpr int fault_count = _DiOS_SF_Last;

    struct MallocNofailGuard {
    private:
        MallocNofailGuard( FaultBase & f )
            : _f( f ),
              _config( _f.config() ),
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

        FaultBase& _f;
        uint8_t *_config;
        uint8_t _s;

        friend FaultBase;
    };

    MallocNofailGuard makeMallocNofail()
    {
        return { *this };
    }

    FaultBase() : _flags( 0 ) {}

    template < typename Context >
    static __trapfn void handler( _VM_Fault _what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept
    {
        uint64_t old = __vm_ctl_flag( 0, _VM_CF_KernelMode | _VM_CF_IgnoreCrit | _VM_CF_IgnoreLoop |
                                         _DiOS_CF_IgnoreFault );

        if ( old & _DiOS_CF_IgnoreFault )
            goto ret;

        __dios_sync_parent_frame();

        {
            bool kernel = old & _VM_CF_KernelMode;
            auto *frame = __dios_parent_frame();
            auto& fault = get_state< Context >();
            fault.fault_handler( kernel, frame, _what );
        }

        // Continue if we get the control back
        old |= uint64_t( __vm_ctl_get( _VM_CR_Flags ) ) & ( _DiOS_CF_Fault | _VM_CF_Error );
        // clean possible intermediate frames to avoid memory leaks
        __dios_stack_free( __dios_parent_frame(), cont_frame );
      ret:
        __vm_ctl_set( _VM_CR_Flags, reinterpret_cast< void * >( old ) );
        __vm_ctl_set( _VM_CR_Frame, cont_frame, cont_pc );
        __builtin_trap();
    }

    void updateSimFail( uint8_t *config = nullptr );
    void backtrace( _VM_Frame * frame );
    void fault_handler( int kernel, _VM_Frame * frame, int what );

    static int str_to_fault( std::string_view fault );
    static std::string_view fault_to_str( int f, bool ext = false );
    void trace_config( int indent );
    void load_user_pref( uint8_t *config, SysOpts& opts );
    int _faultToStatus( int fault );
    int configure_fault( int fault, int cfg );
    int get_fault_config( int fault );
};

template < typename Next >
struct Fault: FaultBase, Next
{

    struct Process : Next::Process, ProcessFaultInfo {};

    template< typename Setup >
    void setup( Setup s )
    {
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

    void getHelp( ArrayMap< std::string_view, HelpOption >& options )
    {
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

    uint8_t *config() override
    {
        auto *task = Next::current_task();
        if ( !task )
            return nullptr;
        return static_cast< Process * >( task->_proc )->faultConfig;
    }

    void die() override { Next::die(); }

    using FaultBase::get_fault_config;
    using FaultBase::configure_fault;
    using FaultBase::fault_handler;
};

} // namespace __dios

#endif // __cplusplus

#endif // __DIOS_FAULT_H__
