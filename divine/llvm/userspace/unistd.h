#include <sys/types.h>

#ifndef _UNISTD_H_
#define _UNISTD_H_

#define F_OK        0
#define X_OK        1
#define W_OK        2
#define R_OK        4

/* Standard file descriptors.  */
#define STDIN_FILENO  0  /* Standard input.  */
#define STDOUT_FILENO 1  /* Standard output.  */
#define STDERR_FILENO 2  /* Standard error output.  */

#if !defined( SEEK_SET )
    #define SEEK_SET 0  /* Seek from beginning of file.  */
#elif SEEK_SET != 0
    #error SEEK_SET != 0
#endif

#if !defined( SEEK_CUR )
    #define SEEK_CUR 1  /* Seek from current position.  */
#elif SEEK_CUR != 1
    #error SEEK_CUR != 1
#endif

#if !defined( SEEK_END )
    #define SEEK_END 2  /* Seek from end of file.  */
#elif SEEK_END != 2
    #error SEEK_END != 2
#endif

#if !defined( SEEK_DATA )
    #define SEEK_DATA 3
#elif SEEK_DATA != 3
    #error SEEK_DATA != 3
#endif

#if !defined( SEEK_HOLE )
    #define SEEK_HOLE 4
#elif SEEK_HOLE != 4
    #error SEEK_HOLE != 4
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FS_NOINLINE __attribute__((noinline))

FS_NOINLINE int close( int fd );
FS_NOINLINE ssize_t read( int fd, void *buf, size_t count );
FS_NOINLINE ssize_t write( int fd, const void *buf, size_t count );

FS_NOINLINE ssize_t pread( int fd, void *buf, size_t count, off_t offset );
FS_NOINLINE ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset );

FS_NOINLINE int pipe( int pipefd[ 2 ] );

FS_NOINLINE off_t lseek( int fd, off_t offset, int whence );

FS_NOINLINE int dup( int fd );
FS_NOINLINE int dup2( int oldfd, int newfd );

FS_NOINLINE int ftruncate( int fd, off_t length );
FS_NOINLINE int truncate( const char *path, off_t length );

FS_NOINLINE int unlink( const char *path );
FS_NOINLINE int rmdir( const char *path );
FS_NOINLINE int unlinkat( int dirfd, const char *path, int flags );

FS_NOINLINE int link( const char *target, const char *linkpath );
FS_NOINLINE int linkat( int olddirfd, const char *target, int newdirfd, const char *linkpath, int flags );
FS_NOINLINE int symlink( const char *target, const char *linkpath );
FS_NOINLINE int symlinkat( const char *target, int dirfd, const char *linkpath );

FS_NOINLINE ssize_t readlink( const char *path, char *buf, size_t count );
FS_NOINLINE ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count );

FS_NOINLINE int access( const char *path, int mode );
FS_NOINLINE int faccessat( int dirfd, const char *path, int mode, int flags );

FS_NOINLINE int chdir( const char *path );
FS_NOINLINE int fchdir( int dirfd );

void _exit( int status );

FS_NOINLINE int fsync( int fd );
FS_NOINLINE int fdatasync( int fd );

FS_NOINLINE void swab( const void *from, void *to, ssize_t n );

FS_NOINLINE int isatty( int fd );
FS_NOINLINE char *ttyname( int fd );
FS_NOINLINE int ttyname_r( int fd, char *buf, size_t count );

FS_NOINLINE void sync( void );
FS_NOINLINE int syncfs( int fd );

int nanosleep( const struct timespec *req, struct timespec *rem );
unsigned int sleep( unsigned int seconds );
int usleep( useconds_t usec );

// entrypoints for rename & renameat functions
FS_NOINLINE int _FS_renameitemat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath );
FS_NOINLINE int _FS_renameitem( const char *oldpath, const char *newpath );

#undef FS_NOINLINE

#ifdef __cplusplus
} // extern C
#endif

#endif

