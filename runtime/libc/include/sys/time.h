#ifndef __DIVINE_TIME_H__
#define __DIVINE_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

struct timeval
{
    long tv_sec;    /* seconds */
    long tv_usec;   /* microseconds */
};

int gettimeofday( struct timeval *tp, void *tzp );

#ifdef __cplusplus
} // extern "C"
#endif
#endif // __DIVINE_TIME_H__
