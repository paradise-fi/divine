#include <sys/types.h>
#include <sys/syscall.h>
#include <dios/macro/no_memory_tags>

#define SYSCALL( name, schedule, ret, arg ) ret SysProxy::name arg noexcept { __builtin_trap(); }

namespace __dios
{
    #include <sys/syscall.def>
}

#undef SYSCALL
#include <dios/macro/no_memory_tags.cleanup>

extern "C" void __dios_boot( const _VM_Env *env );

extern "C" void __boot( const _VM_Env *env )
{
    __dios_boot( env );
    __vm_suspend();
}
