// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

#include <_PDCLIB/cdefs.h>
#include <stdint.h>
#include <sys/metadata.h>
#include <sys/divm.h>

#ifndef _SETJMP_H_
#define _SETJMP_H_

_PDCLIB_EXTERN_C

typedef uint64_t jmp_buf[ _HOST_jmp_buf_size / 8 ];

int setjmp( jmp_buf env ) __attribute__((__noinline__, __returns_twice__, __nothrow__));
// int sigsetjmp( sigjmp_buf env, int savesigs );

void longjmp( jmp_buf env, int val ) __attribute__((__nothrow__));
// void siglongjmp( sigjmp_buf env, int val );

_PDCLIB_EXTERN_END

#endif // _SETJMP_H_
