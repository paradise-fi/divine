#ifndef _PDCLIB_UNISTD_H
#define _PDCLIB_UNISTD_H

#include <_PDCLIB_int.h>

typedef unsigned ssize_t;
typedef   signed off_t;

_PDCLIB_BEGIN_EXTERN_C

off_t lseek(int fd, off_t offset, int whence);

_PDCLIB_END_EXTERN_C

#endif
