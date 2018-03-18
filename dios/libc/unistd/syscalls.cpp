#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/monitor.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/argpad.hpp>
#include <signal.h>
#include <dios.h>

#include <brick-tuple>
#include <brick-hlist>
#include <dios/macro/no_memory_tags>

namespace __dios
{

/* NB. The extern declaration of this on the kernel side has real void return
 * types.  This is a bit of a dirty trick. We fiddle around with long jumps to
 * avoid void returns into non-void callsites. */

#define VOID int
#define SYSCALL( name, schedule, ret, arg )    ret (*name ## _ptr) arg noexcept;
#include <sys/syscall.def>
#undef VOID

#undef SYSCALL

#define SYSCALL_DIOS(...)
#define SYSCALL( name, schedule, ret, arg )                                   \
    extern "C" __trapfn ret name arg noexcept                                 \
    {                                                                         \
        return unpad( __dios::name ## _ptr, _1, _2, _3, _4, _5, _6 );         \
    }
#include <sys/syscall.def>

#undef SYSCALL
#undef SYSCALL_DIOS
#define SYSCALL(...)
#define SYSCALL_DIOS( name, schedule, ret, arg )                              \
    extern "C" __trapfn ret __dios_ ## name arg noexcept                      \
    {                                                                         \
        return unpad( __dios::name ## _ptr, _1, _2, _3, _4, _5, _6 );         \
    }
#include <sys/syscall.def>
#undef SYSCALL_DIOS
#undef SYSCALL

    template< typename Todo, typename Done, typename Ret, typename... Args >
    __inline static auto unpack( Done done, Ret (*f)( Args... ), va_list vl,
                                 typename Todo::Empty = typename Todo::Empty()  )
    {
        return brick::tuple::pass( [&]( auto... x ) { return f( x... ); }, done );
    }

    template< typename Todo, typename Done, typename Ret, typename... Args >
    __inline static auto unpack( Done done, Ret (*f)( Args... ), va_list vl,
                                 typename Todo::Head = typename Todo::Head()  )
    {
        auto next = std::tuple_cat( done, std::make_tuple( va_arg( vl, typename Todo::Head ) ) );
        return unpack< typename Todo::Tail >( next, f, vl );
    }

    template< typename Ret, typename... Args >
    __inline static auto unpack( Ret (*f)( Args... ), va_list vl )
    {
        return unpack< brick::hlist::TypeList< Args... > >( std::make_tuple(), f, vl );
    }

#define VOID int

    extern "C" long syscall( int id, ... ) noexcept
    {
        va_list ap;
        va_start( ap, id );
        switch ( id )
        {
#define SYSCALL( name, schedule, ret, arg ) \
            case SYS_ ## name: return long( unpack( name ## _ptr, ap ) );
#include <sys/syscall.def>
            default:
                __dios_fault( _DiOS_F_Syscall, "bad syscall number" );
                errno = ENOSYS;
                return -1;
        }
    }

#undef VOID

#include <dios/macro/no_memory_tags.cleanup>
#undef SYSCALL

}
