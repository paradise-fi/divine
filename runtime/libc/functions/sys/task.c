#ifndef _SYS_INTERRUPT_H
#define _SYS_INTERRUPT_H

#include <sys/task.h>

void __dios_sync_entry( void *_fun )
{
    void (*fun)( void ) = _fun;
    while ( 1 )
    {
        fun();
        __dios_yield();
    }
}

void __dios_sync_task( void (*entry)( void ) ) __nothrow
{
    __dios_start_task( __dios_sync_entry, entry, 0 );
}

#endif
