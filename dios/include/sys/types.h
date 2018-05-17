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

typedef __int64_t __blkcnt_t;      /* blocks allocated for file */
typedef __int32_t __blksize_t;     /* optimal blocksize for I/O */
typedef __int64_t __clock_t;       /* ticks in CLOCKS_PER_SEC */
typedef __int32_t __clockid_t;     /* CLOCK_* identifiers */
typedef unsigned long __cpuid_t;   /* CPU id */
typedef __int32_t __dev_t;         /* device number */
typedef __uint32_t __fixpt_t;      /* fixed point number */
typedef __uint64_t __fsblkcnt_t;   /* file system block count */
typedef __uint64_t __fsfilcnt_t;   /* file system file count */
typedef __uint32_t __gid_t;        /* group id */
typedef __uint32_t __id_t;         /* may contain pid, uid or gid */
typedef __uint32_t __in_addr_t;    /* base type for internet address */
typedef __uint16_t __in_port_t;    /* IP port type */
typedef __uint64_t __ino_t;        /* inode number */
typedef long __key_t;              /* IPC key (for Sys V IPC) */
typedef __uint32_t __mode_t;       /* permissions */
typedef __uint32_t __nlink_t;      /* link count */
typedef __int64_t __off_t;         /* file offset or size */
typedef __int32_t __pid_t;         /* process id */
typedef __uint64_t __rlim_t;       /* resource limit */
typedef __uint8_t __sa_family_t;   /* sockaddr address family type */
typedef __int32_t __segsz_t;       /* segment size */
typedef __uint32_t __socklen_t;    /* length type for network syscalls */
typedef long __suseconds_t;        /* microseconds (signed) */
typedef __int32_t __swblk_t;       /* swap offset */
typedef __int64_t __time_t;        /* epoch time */
typedef __int32_t __timer_t;       /* POSIX timer identifiers */
typedef __uint32_t __uid_t;        /* user id */
typedef __uint32_t __useconds_t;   /* microseconds */
typedef __suseconds_t suseconds_t; /* microseconds (signed) */

#endif // _SYS_TYPES_H_
