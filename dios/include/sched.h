#ifndef _SCHED_H
#define _SCHED_H

#include <sys/divm.h>
#include <time.h> /* struct timespec */

#ifdef __cplusplus
extern "C"
#endif
int sched_yield( void ) __nothrow;

struct sched_param {
    int sched_priority;
#if defined(_POSIX_SPORADIC_SERVER) || defined(_POSIX_THREAD_SPORADIC_SERVER)
    int sched_ss_low_priority;
    struct timespec sched_ss_repl_period;
    struct timespec sched_ss_init_budget;
    int sched_ss_max_repl;
#endif
};

#endif
