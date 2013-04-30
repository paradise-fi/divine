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

typedef int pid_t;

typedef unsigned int clockid_t;
typedef unsigned int time_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct sched_param {
    int sched_priority;
#if defined(_POSIX_SPORADIC_SERVER) || defined(_POSIX_THREAD_SPORADIC_SERVER)
    int sched_ss_low_priority;
    struct timespec sched_ss_repl_period;
    struct timespec sched_ss_init_budget;
    int sched_ss_max_repl;
#endif
};

typedef unsigned long size_t;

/* Function prototypes */

#ifdef __cplusplus
extern "C" {
#endif

/* Memory allocation */
void * malloc( size_t size ) NOINLINE;
void * calloc( size_t size ) NOINLINE;
void * realloc( void *ptr, size_t size ) NOINLINE;
void free( void * ) NOINLINE;

/* Operators new & delete */
void _Znwm( void );
void _Znam( void );

/* IOStream */
void _ZNSt8ios_base4InitC1Ev( void );

/* Exit */
void exit( int );
int __cxa_atexit( void ( * ) ( void * ), void *, void * );
int atexit( void ( * )( void ) );

#ifdef __cplusplus
}
#endif

#endif
