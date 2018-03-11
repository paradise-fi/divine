#ifndef __SYS_SIGNAL_H__
#define __SYS_SIGNAL_H__

#include <_PDCLIB_aux.h>

_PDCLIB_EXTERN_C

int __dios_signal_trampoline_ret( void *, void *, int );
int __dios_signal_trampoline_noret( void *, void *, int );
int __dios_simple_trampoline( void *, void *, int );

_PDCLIB_EXTERN_END

#endif
