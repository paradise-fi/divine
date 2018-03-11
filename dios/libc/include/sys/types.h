// -*- C++ -*- (c) 2015 Jiří Weiser

#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include <bits/types.h>
#include <sys/hostabi.h>

// size_t
#include <stddef.h>

typedef __intptr_t          ssize_t;

typedef __int32_t           pid_t;


typedef __uintptr_t         ptr_t;
typedef __uint32_t          socklen_t;

typedef __uint32_t          uid_t;
typedef __uint32_t          gid_t;

typedef long long           off_t;
typedef __int64_t           off64_t;

typedef __uint64_t          dev_t;
typedef __uint64_t          ino_t;

typedef __uint64_t          nlink_t;
typedef __uint64_t          blksize_t;
typedef __uint64_t          blkcnt_t;

/* Convenience types.  */
typedef __u_char            u_char;
typedef __u_short           u_short;
typedef __u_int             u_int;
typedef __u_long            u_long;
typedef __uint16_t          u_int16_t;
typedef __uint32_t          u_int32_t;

typedef	double __double_t;
typedef	float  __float_t;

typedef __uint32_t fixpt_t;
typedef _HOST_mode_t mode_t;

#endif // _SYS_TYPES_H_
