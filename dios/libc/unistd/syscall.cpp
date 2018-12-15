#define _DIOS_NORM_SYSCALLS
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/monitor.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <sys/argpad.hpp>
#include <fcntl.h>
#include <signal.h>
#include <dios.h>

#include <brick-tuple>
#include <brick-hlist>
#include <dios/macro/no_memory_tags>

namespace __dios
{

    template< typename Todo, typename Done, typename Ret, typename... Args >
    __inline static auto unpack( Done done, Ret (*f)( Args... ), va_list vl,
                                 typename Todo::Empty = typename Todo::Empty()  )
    {
        return brick::tuple::pass( [&]( auto... x ) { return f( x... ); }, done );
    }

    template< typename Todo, typename Done, typename... Args >
    __inline static long unpack( Done done, void (*f)( Args... ), va_list vl,
                                 typename Todo::Empty = typename Todo::Empty()  )
    {
        brick::tuple::pass( [&]( auto... x ) { f( x... ); }, done );
        return 0;
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

    extern "C" long syscall( int id, ... ) noexcept
    {
        va_list ap;
        va_start( ap, id );
        switch ( id )
        {
#define SYSCALL_DIOS( name, schedule, ret, arg ) \
            case SYS_ ## name: return long( unpack( __dios_ ## name, ap ) );
#define SYSCALL( name, schedule, ret, arg ) \
            case SYS_ ## name: return long( unpack( name, ap ) );
#include <sys/syscall.def>
            default:
                __dios_fault( _DiOS_F_Syscall, "bad syscall number" );
                errno = ENOSYS;
                return -1;
        }
    }

#include <dios/macro/no_memory_tags.cleanup>
#undef SYSCALL

}
