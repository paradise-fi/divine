#ifndef _SYS_INTERRUPT_H
#define _SYS_INTERRUPT_H

#include <sys/interrupt.h>

__attribute__((__annotate__("divine.link.always")))
int __dios_mask( int set )
{
    int old;
    if ( set )
        old = __vm_ctl_flag( 0, _DiOS_CF_Mask ) & _DiOS_CF_Mask;
    else
        old = __vm_ctl_flag( _DiOS_CF_Mask, 0 ) & _DiOS_CF_Mask;
    return old ? 1 : 0;
}

#endif
