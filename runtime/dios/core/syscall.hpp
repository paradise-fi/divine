// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <cstdarg>
#include <new>
#include <dios.h>
#include <errno.h>
#include <dios/kernel.hpp>
#include <sys/syscall.h>

// Syscall argument types
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/monitor.h>
#include <sys/argpad.hpp>
#include <signal.h>

namespace __dios
{

#include <dios/macro/no_memory_tags>
#define SYSCALL( name, schedule, ret, arg )    extern ret (*name ## _ptr) arg;
#include <sys/syscall.def>
#undef SYSCALL
#include <dios/macro/no_memory_tags.cleanup>

struct SetupBase
{
    MemoryPool *pool;
    const _VM_Env *env;
    SysOpts opts;
};

template< typename Context_ >
struct Setup : SetupBase
{
    using Context = Context_;
    typename Context::Process *proc1;

    Setup( const SetupBase &s ) : SetupBase( s ) {}
};

struct Trap
{
    enum RM { CONTINUE, RESCHEDULE, TRAMPOLINE } retmode;
    uint64_t keepflags;
    static const uint64_t kern = _VM_CF_KernelMode | _VM_CF_IgnoreLoop | _VM_CF_IgnoreCrit;

    __inline Trap( RM rm ) : retmode( rm )
    {
        keepflags = __vm_ctl_flag( 0, kern ) & kern;
    }

    __inline ~Trap()
    {
        __vm_ctl_flag( kern & ~keepflags, 0 );
        if ( retmode == RESCHEDULE )
            __dios_interrupt();
        if ( retmode == TRAMPOLINE )
            __dios_set_frame( __dios_this_frame()->parent );
    }
};

struct BaseContext
{
    using SyscallInvoker = void (*)( void *, _DiOS_SC syscode, void *, va_list );

    struct Process
    {
        virtual ~Process() {}
    };

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< BaseContext >( "{BaseContext}" );
        using Ctx = typename Setup::Context;

        #include <dios/macro/no_memory_tags>
        #define SYSCALL( name, schedule, ret, arg ) name ## _ptr = [] arg __trapfn -> ret \
        {                                                                                 \
            Trap _trap( Trap::schedule );                                                 \
            return unpad( []( auto... t ) __inline -> ret                                 \
            {                                                                             \
                auto ctx = reinterpret_cast< Ctx * >( __vm_ctl_get( _VM_CR_State ) );     \
                return ctx->name( t... );                                                 \
            }, _1, _2, _3, _4, _5, _6 );                                                  \
        };

        #include <sys/syscall.def>

        #undef SYSCALL
        #include <dios/macro/no_memory_tags.cleanup>

        if ( s.opts.empty() )
            return;
        for ( const auto& opt : s.opts )
            __dios_trace_f( "ERROR: Unused option %s:%s", opt.first.c_str(), opt.second.c_str() );
        __dios_fault( _DiOS_F_Config, "Unused options" );
    }

    void finalize() {}

    void getHelp( Map< String, HelpOption >& ) {}

    #include <dios/macro/no_memory_tags>
    #define SYSCALL( name, schedule, ret, arg ) ret name arg;

    #include <sys/syscall.def>

    #undef SYSCALL
    #include <dios/macro/no_memory_tags.cleanup>
};

}


#endif // __DIOS_SYSCALL_H__
