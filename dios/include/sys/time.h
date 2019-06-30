#ifndef __DIVINE_TIME_H__
#define __DIVINE_TIME_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifndef _TIMEVAL_DECLARED
#define _TIMEVAL_DECLARED
struct timeval
{
    long tv_sec;    /* seconds */
    long tv_usec;   /* microseconds */
};
#endif

struct timezone
{
    int tz_minuteswest; /* minutes west of Greenwich */
    int tz_dsttime; /* type of dst correction */
};

int gettimeofday( struct timeval *tp, struct timezone *tzp ) __nothrow;
int settimeofday( const struct timeval *tp, const struct timezone *tzp ) __nothrow;

int utimes( const char *filename, const struct timeval times[2] );

__END_DECLS

#endif // __DIVINE_TIME_H__
