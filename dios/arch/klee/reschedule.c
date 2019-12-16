#include <sys/interrupt.h>

__link_always __trapfn __invisible __weakmem_direct void __dios_reschedule()
{
    /* nothing */
}

__link_always __trapfn __invisible __weakmem_direct void __dios_suspend()
{
    __vm_suspend();
}
