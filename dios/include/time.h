/* Date and time <time.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_TIME_H
#define _PDCLIB_TIME_H _PDCLIB_TIME_H
#include "_PDCLIB/cdefs.h"
#include "_PDCLIB/int.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PDCLIB_SIZE_T_DEFINED
#define _PDCLIB_SIZE_T_DEFINED _PDCLIB_SIZE_T_DEFINED
typedef _PDCLIB_size_t size_t;
#endif

#ifndef _PDCLIB_NULL_DEFINED
#define _PDCLIB_NULL_DEFINED _PDCLIB_NULL_DEFINED
#define NULL _PDCLIB_NULL
#endif

#ifndef _TIME_T_DEFINED_
#define _TIME_T_DEFINED_ 1
typedef _PDCLIB_time_t  time_t;
#endif

typedef _PDCLIB_clock_t clock_t;

#define CLOCKS_PER_SEC _PDCLIB_CLOCKS_PER_SEC
#define TIME_UTC _PDCLIB_TIME_UTC

#ifndef _TIMESPEC_DECLARED
#define _TIMESPEC_DECLARED
struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};
#endif

struct tm
{
    int tm_sec;   /* 0-60 */
    int tm_min;   /* 0-59 */
    int tm_hour;  /* 0-23 */
    int tm_mday;  /* 1-31 */
    int tm_mon;   /* 0-11 */
    int tm_year;  /* years since 1900 */
    int tm_wday;  /* 0-6 */
    int tm_yday;  /* 0-365 */
    int tm_isdst; /* >0 DST, 0 no DST, <0 information unavailable */
    long tm_gmtoff; /* offset from UTC in seconds */
    char *tm_zone;  /* abbreviation of timezone name */
};

/* Returns the number of "clocks" in processor time since the invocation
   of the program. Divide by CLOCKS_PER_SEC to get the value in seconds.
   Returns -1 if the value cannot be represented in the return type or is
   not available.
*/
clock_t clock( void ) _PDCLIB_nothrow;

/* Returns the difference between two calendar times in seconds. */
double difftime( time_t time1, time_t time0 ) _PDCLIB_nothrow;

time_t mktime( struct tm * timeptr ) _PDCLIB_nothrow;

time_t time( time_t * timer ) _PDCLIB_nothrow;

int timespec_get( struct timespec * ts, int base ) _PDCLIB_nothrow;

char * asctime( const struct tm * timeptr ) _PDCLIB_nothrow;
char * asctime_r( const struct tm *timeptr, char *buf ) _PDCLIB_nothrow;

char * ctime( const time_t * timer ) _PDCLIB_nothrow;

struct tm * gmtime( const time_t * timer ) _PDCLIB_nothrow;

struct tm * localtime( const time_t * timer ) _PDCLIB_nothrow;

size_t strftime( char * _PDCLIB_restrict s, size_t maxsize, const char * _PDCLIB_restrict format, const struct tm * _PDCLIB_restrict timeptr );

#define CLOCK_REALTIME              1
#define CLOCK_MONOTONIC             2
#define CLOCK_PROCESS_CPUTIME_ID    3
#define CLOCK_THREAD_CPUTIME_ID     4

typedef unsigned int clockid_t;

/* added for DIVINE */
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(clockid_t clk_id, struct timespec *tp);

#ifdef __cplusplus
}
#endif
#endif
