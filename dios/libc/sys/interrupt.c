#ifndef _SYS_INTERRUPT_H
#define _SYS_INTERRUPT_H

#include <sys/interrupt.h>

__attribute__((__annotate__("divine.link.always")))
int __dios_mask( int set )
{
    uint64_t old;
    if ( set )
        old = __vm_ctl_flag( 0, _DiOS_CF_Mask );
    else
    {
        old = __vm_ctl_flag( _DiOS_CF_Mask, 0 );
        if ( old & _DiOS_CF_Deferred )
            __dios_reschedule();
    }
    return ( old & _DiOS_CF_Mask ) ? 1 : 0;
}

#endif
