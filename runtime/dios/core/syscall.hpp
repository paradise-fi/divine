// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#ifdef __dios_kernel__
#define BRICK_TUPLE_INLINE_PASS
#include <brick-tuple>
#include <brick-hlist>
#undef BRICK_TUPLE_INLINE_PASS
#endif

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
#include <signal.h>

namespace __dios {

#ifdef __dios_kernel__

template < typename Context >
struct Syscall
{
    using ScHandler = void (*)( Context& c, void *, va_list );

    static void handle( Context& c, _DiOS_Syscall& s ) noexcept
    {
        if ( s._syscode != SYS_NONE )
        {
            ( *( table[ s._syscode ] ) )( c, s._ret, s._args );
            s._syscode = SYS_NONE;
            if ( !c.need_reschedule() && *__dios_get_errno() == EAGAIN2 )
                c.reschedule();
        }
    }

    static void kernelHandle( void *ctx, _DiOS_SC syscode, void *ret, va_list vl ) noexcept {
        Context& c = *static_cast< Context* >( ctx );
        ( *( table[ syscode ] ) )( c, ret, vl );
    }

    static ScHandler table[ SYS_MAXSYSCALL ];
    template< typename T > using Void = void;

    template< typename Rem, typename Done, typename S, typename T, typename... Args >
    __attribute__(( __always_inline__, __flatten__))
    static auto unpack( Done d, Context &c, T (S::*f)( Args... ), void *rv, va_list vl, int = 0 )
        -> typename std::enable_if< !std::is_same< T, Void< typename Rem::Empty > >::value >::type
    {
        auto rvt = reinterpret_cast< T* >( rv );
        *rvt = brick::tuple::pass( [&]( auto... x ) { return (c.*f)( x... ); }, d );
    }

    template< typename Rem, typename Done, typename S, typename T, typename... Args >
    __attribute__(( __always_inline__, __flatten__))
    static auto unpack( Done d, Context &c, T (S::*f)( Args... ), void *rv, va_list vl, int = 0 )
        -> typename std::enable_if< std::is_same< T, Void< typename Rem::Empty > >::value >::type
    {
        brick::tuple::pass( [&]( auto... x ) { return (c.*f)( x... ); }, d );
    }

    template< typename Rem, typename Done, typename S, typename T, typename... Args >
    __attribute__(( __always_inline__, __flatten__))
    static auto unpack( Done d, Context &c, T (S::*f)( Args... ), void *rv, va_list vl )
        -> Void< typename Rem::Head >
    {
        auto next = std::tuple_cat( d, std::make_tuple( va_arg( vl, typename Rem::Head ) ) );
        unpack< typename Rem::Tail >( next, c, f, rv, vl );
    }

    template< typename S, typename T, typename... Args >
    __attribute__(( __always_inline__, __flatten__))
    static void unpack( Context &c, T (S::*f)( Args... ), void *rv, va_list vl )
    {
        unpack< brick::hlist::TypeList< Args... > >( std::make_tuple(), c, f, rv, vl );
    }

    #include <dios/macro/no_memory_tags>
    #define SYSCALL( name, schedule, ret, arg )                                      \
        __attribute__(( __always_inline__, __flatten__))                             \
        static void name ## Wrappper( Context& ctx, void *rv, va_list vl )           \
        {                                                                            \
            const uint64_t CONTINUE = 0, RESCHEDULE = _DiOS_CF_Reschedule;           \
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _DiOS_CF_Reschedule, schedule ); \
            unpack( ctx, &Context::name, rv, vl );                                   \
        }
    #define SYSCALLSEP SYSCALL
    #include <sys/syscall.def>
    #undef SYSCALLSEP
    #undef SYSCALL
    #include <dios/macro/no_memory_tags.cleanup>
};

template < typename Context >
typename Syscall< Context >::ScHandler Syscall< Context >::table[ SYS_MAXSYSCALL ] = {
    #define SYSCALL( name, ... ) [ SYS_ ## name ] = Syscall:: name ## Wrappper,
    #define SYSCALLSEP SYSCALL
        #include <sys/syscall.def>
    #undef SYSCALL
    #undef SYSCALLSEP
};

#endif

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

struct BaseContext
{
    using SyscallInvoker = void (*)( void *, _DiOS_SC syscode, void *, va_list );

    struct Process
    {
        virtual ~Process() {}
    };

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< BaseContext >( "{BaseContext}" );
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
    #define SYSCALLSEP( name, schedule, ret, arg ) ret name arg;

    #include <sys/syscall.def>

    #undef SYSCALL
    #undef SYSCALLSEP
    #include <dios/macro/no_memory_tags.cleanup>
};

} // namespace __dios


#endif // __DIOS_SYSCALL_H__
