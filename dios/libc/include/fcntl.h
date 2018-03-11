// -*- C++ -*- (c) 2015 Jiří Weiser

#ifndef _FCNTL_H_
#define _FCNTL_H_

#include <sys/types.h>
#include <sys/hostabi.h>

#define O_RDONLY   _HOST_O_RDONLY
#define O_WRONLY   _HOST_O_WRONLY
#define O_RDWR     _HOST_O_RDWR
#define O_CREAT    _HOST_O_CREAT
#define O_EXCL     _HOST_O_EXCL
#define O_NOCTTY   _HOST_O_NOCTTY
#define O_TRUNC    _HOST_O_TRUNC
#define O_APPEND   _HOST_O_APPEND
#define O_NONBLOCK _HOST_O_NONBLOCK
#define O_NDELAY   O_NONBLOCK

#define O_DSYNC        010000  /* Synchronize data.  */
#define O_ASYNC        020000
#define O_SYNC       04010000
#define O_FSYNC        O_SYNC

#define O_RSYNC        O_SYNC  /* Synchronize read operations.   */

#define O_DIRECT       040000  /* Direct disk access.  */
#define O_DIRECTORY   0200000  /* Must be a directory.   */
#define O_NOFOLLOW    0400000  /* Do not follow links.   */
#define O_NOATIME    01000000  /* Do not set atime.  */
#define O_CLOEXEC    02000000  /* Set close_on_exec.  */
#define O_PATH      010000000  /* Resolve pathname but do not open file.  */

#define O_LARGEFILE   0100000

/* Bits in the file status flags returned by F_GETFL.
   These are all the O_* flags, plus FREAD and FWRITE, which are
   independent bits set by which of O_RDONLY, O_WRONLY, and O_RDWR, was
   given to `open'.  */
#define FREAD     1
#define FWRITE    2

/* Traditional BSD names the O_* bits.  */
#define FASYNC    O_ASYNC
#define FFSYNC    O_FSYNC
#define FSYNC     O_SYNC
#define FAPPEND   O_APPEND
#define FNDELAY   O_NDELAY

/* Mask for file access modes.  This is system-dependent in case
   some system ever wants to define some other flavor of access.  */
#define O_ACCMODE  (O_RDONLY|O_WRONLY|O_RDWR)

/* Values for the second argument to `fcntl'.  */
#define F_DUPFD         0  /* Duplicate file descriptor.  */
#define F_GETFD         1  /* Get file descriptor flags.  */
#define F_SETFD         2  /* Set file descriptor flags.  */
#define F_GETFL         3  /* Get file status flags.  */
#define F_SETFL         4  /* Set file status flags.  */
#define F_GETOWN        5  /* Get owner (receiver of SIGIO).  */
#define F_SETOWN        6  /* Set owner (receiver of SIGIO).  */
#define F_GETLK         7  /* Get record locking info.  */
#define F_SETLK         8  /* Set record locking info (non-blocking).  */
#define F_SETLKW        9  /* Set record locking info (blocking).  */
/* Not necessary, we always have 64-bit offsets.  */
#define F_GETLK64       F_GETLK  /* Get record locking info.  */
#define F_SETLK64       F_SETLK  /* Set record locking info (non-blocking).  */
#define F_SETLKW64      F_SETLKW/* Set record locking info (blocking).  */
#define F_DUPFD_CLOEXEC 12  /* Duplicate file descriptor with close-on-exit set.  */

/* File descriptor flags used with F_GETFD and F_SETFD.  */
#define FD_CLOEXEC  1  /* Close on exec.  */

#ifndef R_OK      /* Verbatim from <unistd.h>.  Ugh.  */
/* Values for the second argument to access.
   These may be OR'd together.  */
# define F_OK   0       /* Test for existence.  */
# define X_OK   1       /* Test for execute permission.  */
# define W_OK   2       /* Test for write permission.  */
# define R_OK   4       /* Test for read permission.  */
#endif

#define AT_FDCWD              -100   /* Special value used to indicate
                                        the *at functions should use the
                                        current working directory. */
#define AT_SYMLINK_NOFOLLOW   0x100  /* Do not follow symbolic links.  */
#define AT_REMOVEDIR          0x200  /* Remove directory instead of
                                        unlinking file.  */
#define AT_SYMLINK_FOLLOW     0x400  /* Follow symbolic links.  */
#define AT_NO_AUTOMOUNT       0x800  /* Suppress terminal automount traversal.  */
#define AT_EMPTY_PATH        0x1000  /* Allow empty relative pathname.  */
#define AT_EACCESS            0x200  /* Test access permitted for
                                        effective IDs, not real IDs.  */


#ifdef __cplusplus
extern "C" {
#endif

#define FS_NOINLINE __attribute__((noinline))

FS_NOINLINE int creat( const char *path, mode_t mode );
FS_NOINLINE int open( const char *path, int flags, ... );
FS_NOINLINE int openat( int dirfd, const char *path, int flags, ... );

FS_NOINLINE int fcntl( int fd, int cmd, ... );

#undef FS_NOINLINE

#ifdef __cplusplus
}
#endif


#endif
