#include "fs-stat.h"
#include "fs-defs.h"

#ifndef _FS_H_
#define _FS_H_

void showFS();

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  int creat( const char *path, int mode )
 *
 *  Returns:
 *          file descriptor in range [0 - 1024] if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              ENOENT
 *              ENOTDIR
 *              ENAMETOOLONG
 *              ELOOP
 *              ENFILE
 *              EEXIST
 */
int creat( const char *path, int mode );

/**
 *  int open( const char *path, int flags[, int mode ] )
 *  int openat( int dirfd, const char *path, int flags[, int mode ] )
 *
 *  Returns:
 *          file descriptor in range [0 - 1024] if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EINVAL
 *              ENAMETOOLONG
 *              ENOENT
 *              ENOTDIR
 *              ELOOP
 *              ENFILE
 *              EEXIST
 *      openat may give also
 *              EBADF
 */
int open( const char *path, int flags, ... );
int openat( int dirfd, const char *path, int flags, ... );
/**
 *  int close( int fd )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EBADF
 */
int close( int fd );

/**
 *  ssize_t write( int fd, const void *buf, size_t count )
 *
 *  Returns:
 *          number of written bytes
 *          -1 in case of error; possible error codes:
 *              EBADF
 *              EPIPE
 */
ssize_t write( int fd, const void *buf, size_t count );
ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset );

/**
 *  ssize_t read( int fd, void *, size_t count )
 *
 *  Returns:
 *          number of read bytes
 *          -1 in case of error; possible error codes:
 *              EBADF
 */
ssize_t read( int fd, void *buf, size_t count );
ssize_t pread( int fd, void *buf, size_t count, off_t offset );

/**
 *  int mkdir( const char *path, int mode )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              ENOENT
 *              EEXIST
 *              ENAMETOOLONG
 *              EACCES
 *              ENOTDIR
 *              ELOOP
 *      mkdirat may give also:
 *              EBADF
 */
int mkdir( const char *path, int mode );
int mkdirat( int dirfd, const char *path, int mode );

/**
 *  int unlink( const char *path )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EISDIR
 *              ENAMETOOLONG
 *              ELOOP
 *              ENOENT
 *              ENOTDIR
 */
int unlink( const char *path );

/**
 *  int rmdir( const char *path )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *             (EINVAL - not supported now)
 *              ELOOP
 *              ENOENT
 *              ENAMETOOLONG
 *              ENOTDIR
 *              ENOTEMPTY
 */
int rmdir( const char *path );

/**
 *  int unlinkat( int dirfd, const char *path, int flags )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EISDIR
 *             (EINVAL - not supported now)
 *              ELOOP
 *              ENAMETOOLONG
 *              ENOENT
 *              ENOTDIR
 *              ENOTEMPTY
 */
int unlinkat( int dirfd, const char *path, int flags );

/**
 *  off_t lseek( int fd, off_t offset, int whence )
 *
 *  Returns:
 *          resulting offset if succeeded
 *          (off_t)-1 in case of error; possible error codes:
 *              EBADF
 *              EINVAL
 *              EOVERFLOW
 *              ESPIPE
 */
off_t lseek( int fd, off_t offset, int whence );

/**
 *  int dup( int fd )
 *
 *  Returns:
 *          file descriptor in range [0 - 1024]
 *          -1 in case of error; possible error codes:
 *              EBADF
 */
int dup( int fd );

/**
 *  int dup2( int fd )
 *
 *  Returns:
 *          file descriptor in range [0 - 1024]
 *          -1 in case of error; possible error codes:
 *              EBADF
 */
int dup2( int oldfd, int newfd );

/**
 *  int symlink( const char *target, const char *linkpath )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EBADF
 *              EEXIST
 *              ENAMETOOLONG
 *              ELOOP
 *              ENOENT
 *              ENOTDIR
 */
int symlink( const char *target, const char *linkpath );
int symlinkat( const char *target, int dirfd, const char *linkpath );

/**
 *  int link( const char *target, const char *linkpath )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EEXIST
 *              ELOOP
 *              ENAMETOOLONG
 *              ENOENT
 *              ENOTDIR
 *              EPERM
 */
int link( const char *target, const char *linkpath );

/**
 *  int access( const char *path, int mode )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              ELOOP
 *              ENAMETOOLONG
 *              ENOENT
 *              ENOTDIR
 */
int access( const char *path, int mode );
int faccessat( int dirfd, const char *path, int mode, int flags );

/**
 *  int fstat( int fd, struct stat *buf )
 *  int stat( const char *path, struct stat *buf )
 *  int lstat( const char *path, struct stat *buf )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EBADF
 *              ELOOP
 *              ENAMETOOLONG
 *              ENOENT
 *              ENOTDIR
 */
int fstat( int fd, struct stat *buf );
int stat( const char *path, struct stat *buf );
int lstat( const char *path, struct stat *buf );

/**
 *  unsigned umask( unsigned mask )
 *
 *  Returns:
 *          previous mask value
 */
unsigned umask( unsigned mask );

/**
 *  int chdir( const char *path )
 *  int fchdir( int dirfd )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EBADF
 *              ENAMETOOLONG
 *              ELOOP
 *              ENOENT
 *              ENOTDIR
 */
int chdir( const char *path );
int fchdir( int dirfd );

void _exit( int status );

/**
 *  int fdatasync( int fd )
 *  int fsync( int fd )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EBADF
 *  Note:
 *          Synchronization is done together with write operation.
 */
int fsync( int fd );
int fdatasync( int fd );

/**
 *  int ftruncate( int fd, off_t length )
 *  int truncate( const char *path, off_t length )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EBADF
 *              EINVAL
 *              EISDIR
 *              ELOOP
 *              ENAMETOOLONG
 *              ENOENT
 *              ENOTDIR
 */
int ftruncate( int fd, off_t length );
int truncate( const char *path, off_t length );

/**
 *  unsigned sleep( unsigned seconds );
 *
 *  Returns:
 *          0 always
 */
unsigned sleep( unsigned seconds );

/**
 *  ssize_t readlink( const char *path, char *buf, size_t count )
 *  ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count )
 *
 *  Returns:
 *          number of bytes copied into buf if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              EBADF
 *              EINVAL
 *              ELOOP
 *              ENOENT
 *              ENOTDIR
 */
ssize_t readlink( const char *path, char *buf, size_t count );
ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count );

/**
 *  void swab( const void *from, void *to, ssize_t n )
 */
void swab( const void *from, void *to, ssize_t n );

/**
 *  int isatty( int fd )
 *
 *  Returns:
 *          0; possible error codes:
 *              EBADF
 *              EINVAL
 */
int isatty( int fd );

/**
 *  char *ttyname( int fd )
 *  int ttyname_r( int fd, char *buf, size_t count )
 */
char *ttyname( int fd );
int ttyname_r( int fd, char *buf, size_t count );

/**
 *  void sync( void )
 *  int syncfs( int fd )
 *
 *  Returns:
 *          0 if succeeded
 *          -1 in case of error; possible error codes:
 *              EBADF
 */
void sync( void );
int syncfs( int fd );

#ifdef __cplusplus
} // extern C
#endif

#endif