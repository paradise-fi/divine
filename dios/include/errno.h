/* Errors <errno.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_ERRNO_H
#define _PDCLIB_ERRNO_H _PDCLIB_ERRNO_H

#include <_PDCLIB/cdefs.h>
#include <_PDCLIB/errno.h>

_PDCLIB_EXTERN_C
extern int *__errno_location (void) __attribute__ ((__const__));
#define errno (*__errno_location ())
_PDCLIB_EXTERN_END

#endif
