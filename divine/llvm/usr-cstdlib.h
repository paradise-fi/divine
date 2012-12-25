#ifndef USR_CSTDLIB_H
#define USR_CSTDLIB_H

/* Includes */

#include "usr.h"

/* Macros */

#ifndef NULL
#define NULL 0L
#endif

/* Data types */

#ifndef __cplusplus
typedef enum { false = 0, true } bool;
#endif

typedef int clockid_t;
struct timespec {};
struct sched_param {};
typedef unsigned long size_t;

/* Function prototypes */

#ifdef __cplusplus
extern "C" {
#endif

void * malloc( size_t size ) NOINLINE;
void free( void * ) NOINLINE;
void _ZSt9terminatev( void );

#ifdef __cplusplus
}
#endif

#endif
