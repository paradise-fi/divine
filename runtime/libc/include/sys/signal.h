#ifndef __SYS_SIGNAL_H__
#define __SYS_SIGNAL_H__

#include <_PDCLIB_aux.h>

_PDCLIB_EXTERN_C

void __dios_signal_trampoline ( struct _VM_Frame *interrupted, void ( *handler )( int ) );

_PDCLIB_EXTERN_END

#endif
