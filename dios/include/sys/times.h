#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <time.h>
#include <sys/cdefs.h>

struct tms {
       clock_t tms_utime;
       clock_t tms_stime;
       clock_t tms_cutime;
       clock_t tms_cstime;
};

__BEGIN_DECLS

clock_t times(struct tms *tp) __nothrow;

__END_DECLS

#endif
