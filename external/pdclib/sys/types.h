#ifndef TYPES_H
#define TYPES_H

#include <_PDCLIB_int.h>

#ifndef _PDCLIB_SIZE_T_DEFINED    // defined in stdlib.h as well
#define _PDCLIB_SIZE_T_DEFINED _PDCLIB_SIZE_T_DEFINED
typedef _PDCLIB_size_t size_t;
#endif
typedef _PDCLIB_intptr_t ssize_t;

typedef _PDCLIB_int32_t pid_t;

typedef _PDCLIB_uintptr_t ptr_t;

typedef _PDCLIB_uint32_t socklen_t;

typedef _PDCLIB_uint32_t uid_t;
typedef _PDCLIB_uint32_t gid_t;

typedef _PDCLIB_int32_t off_t;
typedef _PDCLIB_int64_t off64_t;

typedef _PDCLIB_uint32_t mode_t;

typedef _PDCLIB_uint32_t dev_t;
typedef _PDCLIB_uint32_t ino_t;

typedef _PDCLIB_uint32_t nlink_t;
typedef _PDCLIB_uint32_t blksize_t;
typedef _PDCLIB_uint32_t blkcnt_t;

/* Convenience types.  */
typedef unsigned char u_char;
typedef unsigned short int u_short;
typedef unsigned int u_int;
typedef unsigned long int u_long;
typedef _PDCLIB_uint16_t u_int16_t;
typedef _PDCLIB_uint32_t u_int32_t;

#endif // TYPES_H
