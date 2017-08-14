#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/monitor.h>
#include <sys/resource.h>
#include <signal.h>
#include <dios.h>

#include <dios/macro/no_memory_tags>

static struct Pad {} _1, _2, _3, _4, _5, _6;
template< typename T >
struct UnVoid
{
    T t;
    T *address() { return &t; }
    T get() { return t; }
};

template<> struct UnVoid< void >
{
    void get() {}
    void *address() { return nullptr; }
};

#define SYSCALL(name, schedule, ret, arg)                                     \
    extern "C" ret name arg {                                                 \
        UnVoid< ret > rv;                                                     \
        __dios_syscall( SYS_ ## name, rv.address(), _1, _2, _3, _4, _5, _6 ); \
        return rv.get();                                                      \
    }

#define SYSCALLSEP(...)

#include <sys/syscall.def>
#include <dios/macro/no_memory_tags.cleanup>

#undef SYSCALL
