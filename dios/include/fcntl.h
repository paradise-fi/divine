// -*- C++ -*- (c) 2015 Jiří Weiser

#ifndef _FCNTL_H_
#define _FCNTL_H_

#include <sys/types.h>
#include <sys/hostabi.h>
#include <sys/cdefs.h>

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
#define O_DIRECTORY _HOST_O_DIRECTORY /* Must be a directory.   */
#define O_NOFOLLOW  _HOST_O_NOFOLLOW    /* Do not follow links.   */
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
#define F_DUPFD         _HOST_F_DUPFD   /* Duplicate file descriptor.  */
#define F_GETFD         _HOST_F_GETFD   /* Get file descriptor flags.  */
#define F_SETFD         _HOST_F_SETFD   /* Set file descriptor flags.  */
#define F_GETFL         _HOST_F_GETFL   /* Get file status flags.  */
#define F_SETFL         _HOST_F_SETFL   /* Set file status flags.  */
#define F_GETOWN        _HOST_F_GETOWN  /* Get owner (receiver of SIGIO).  */
#define F_SETOWN        _HOST_F_SETOWN  /* Set owner (receiver of SIGIO).  */
#define F_GETLK         _HOST_F_GETLK   /* Get record locking info.  */
#define F_SETLK         _HOST_F_SETLK   /* Set record locking info (non-blocking).  */
#define F_SETLKW        _HOST_F_SETLKW  /* Set record locking info (blocking).  */
/* Not necessary, we always have 64-bit offsets.  */
#define F_GETLK64       F_GETLK  /* Get record locking info.  */
#define F_SETLK64       F_SETLK  /* Set record locking info (non-blocking).  */
#define F_SETLKW64      F_SETLKW/* Set record locking info (blocking).  */
#define F_DUPFD_CLOEXEC _HOST_F_DUPFD_CLOEXEC /* Duplicate file descriptor with close-on-exit set.  */

#define F_RDLCK 1
#define F_UNLCK 2
#define F_WRLCK 3

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


struct flock
{
        off_t   l_start;
        off_t   l_len;
        pid_t   l_pid;
        short   l_type;
        short   l_whence;
};

#ifdef __cplusplus
extern "C" {
#endif

#define FS_NOINLINE __attribute__((noinline))

FS_NOINLINE int creat( const char *path, mode_t mode ) __nothrow;
FS_NOINLINE int open( const char *path, int flags, ... ) __nothrow;
#ifdef _DIOS_NORM_SYSCALLS
FS_NOINLINE int openat( int dirfd, const char *path, int flags, mode_t mode ) __nothrow;
#else
FS_NOINLINE int openat( int dirfd, const char *path, int flags, ... ) __nothrow;
#endif

FS_NOINLINE int fcntl( int fd, int cmd, ... ) __nothrow;
FS_NOINLINE int flock( int fd, int cmd ) __nothrow;

#undef FS_NOINLINE

#ifdef __cplusplus
}
#endif


#endif
