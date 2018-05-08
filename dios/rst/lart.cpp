#include <rst/lart.h>
#include <sys/task.h>
#include <sys/divm.h>

extern "C"
{
    uintptr_t __lart_unstash()
    {
        return __dios_this_task()->__rst_stash;
    }

    void __lart_stash( uintptr_t val )
    {
        __dios_this_task()->__rst_stash = val;
    }
}
