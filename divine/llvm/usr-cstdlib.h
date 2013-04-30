#ifndef USR_CSTDLIB_H
#define USR_CSTDLIB_H

/* Includes */

#include "usr.h"
#include <stdlib.h>
#include <stdbool.h>

typedef int pid_t;

typedef unsigned int clockid_t;

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

#endif
