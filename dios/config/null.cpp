#include <dios/config/context.hpp>
#include <dios/macro/no_memory_tags>

#define SYSCALL( name, schedule, ret, arg ) ret SysProxy::name arg noexcept {}

namespace __dios
{
    #include <sys/syscall.def>
}

#undef SYSCALL
#include <dios/macro/no_memory_tags.cleanup>
