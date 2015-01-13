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
 *              ELOOP
 *              ENFILE
 *              EEXIST
 */
int creat( const char *path, int mode );

/**
 *  int open( const char *path, int flags[, int mode ] )
 *
 *  Returns:
 *          file descriptor in range [0 - 1024] if succeeded
 *          -1 in case of error; possible error codes:
 *              EACCES
 *              ENOENT
 *              ENOTDIR
 *              ELOOP
 *              ENFILE
 *              EEXIST
 */
int open( const char *path, int flags, ... );

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

/**
 *  ssize_t read( int fd, void *, size_t count )
 *
 *  Returns:
 *      number of read bytes
 *      -1 in case of error; possible error codes:
 *          EBADF
 */
ssize_t read( int fd, void *buf, size_t count );

/**
 *  int mkdir( const char *path, int mode )
 *
 *  Returns:
 *      0 if succeeded
 *      -1 in case of error; possible error codes:
 *          ENOENT
 *          EEXIST
 *          EACCES
 *          ENOTDIR
 *          ELOOP
 */
int mkdir( const char *path, int mode );

int unlink( const char *path );
int rmdir( const char *path );
int unlinkat( int fd, const char *path, int flags );

off_t lseek( int fd, off_t offset, int whence );
int dup( int fd );
int dup2( int oldfd, int newfd );
int symlink( const char *target, const char *linkpath );
int link( const char *target, const char *linkpath );
int access( const char *path, int mode );

int fstat( int fd, struct stat *buf );
inline int stat( const char *path, struct stat *buf );
inline int lstat( const char *path, struct stat *buf );

unsigned umask( unsigned mask );

int chdir( const char *path );
int fchdir( int fd );

#ifdef __cplusplus
} // extern C
#endif

#endif