#ifndef __DIVINE_TIME_H__
#define __DIVINE_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _TIMEVAL_DECLARED
#define _TIMEVAL_DECLARED
struct timeval
{
    long tv_sec;    /* seconds */
    long tv_usec;   /* microseconds */
};
#endif

int gettimeofday( struct timeval *tp, void *tzp );

#ifdef __cplusplus
} // extern "C"
#endif
#endif // __DIVINE_TIME_H__
