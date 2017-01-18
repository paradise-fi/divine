#ifndef _PDCLIB_TIME_H
#define _PDCLIB_TIME_H
#include <_PDCLIB_aux.h>
#include <_PDCLIB_int.h>

_PDCLIB_BEGIN_EXTERN_C
#ifndef _PDCLIB_SIZE_T_DEFINED
#define _PDCLIB_SIZE_T_DEFINED _PDCLIB_SIZE_T_DEFINED
typedef _PDCLIB_size_t size_t;
#endif

#ifndef _PDCLIB_NULL_DEFINED
#define _PDCLIB_NULL_DEFINED _PDCLIB_NULL_DEFINED
#define NULL _PDCLIB_NULL
#endif

typedef _PDCLIB_time_t  time_t;
typedef _PDCLIB_clock_t clock_t;

#define TIME_UTC _PDCLIB_TIME_UTC

#ifndef _PDCLIB_STRUCT_TIMESPEC_DEFINED
#define _PDCLIB_STRUCT_TIMESPEC_DEFINED
_PDCLIB_DEFINE_STRUCT_TIMESPEC()
#endif

#ifndef _PDCLIB_STRUCT_TM_DEFINED
#define _PDCLIB_STRUCT_TM_DEFINED
_PDCLIB_DEFINE_STRUCT_TM()
#endif

#define CLOCK_REALTIME              1
#define CLOCK_MONOTONIC             2
#define CLOCK_PROCESS_CPUTIME_ID    3
#define CLOCK_THREAD_CPUTIME_ID     4

typedef unsigned int clockid_t;

time_t time( time_t* t ) _PDCLIB_nothrow;
int timespec_get( struct timespec *ts, int base ) _PDCLIB_nothrow;
double difftime ( time_t end, time_t beginning) _PDCLIB_nothrow;

/* added for DIVINE */
clockid_t clock( void );
time_t mktime(struct tm *tm);
char *asctime(const struct tm *tm);
char *ctime(const time_t *timep);
struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);
size_t strftime(char *s, size_t max, const char *format,
                const struct tm *tm);
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(clockid_t clk_id, struct timespec *tp);

_PDCLIB_END_EXTERN_C
#endif
