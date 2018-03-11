// -*- C++ -*- (c) 2015 Jiří Weiser

#include <sys/types.h>

#ifndef _UNISTD_H_
#define _UNISTD_H_

#define _POSIX_VERSION 200809L

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

extern char **environ;

typedef __uint32_t useconds_t;

int close( int fd );
ssize_t read( int fd, void *buf, size_t count );
ssize_t write( int fd, const void *buf, size_t count );

ssize_t pread( int fd, void *buf, size_t count, off_t offset );
ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset );

int pipe( int pipefd[ 2 ] );

off_t lseek( int fd, off_t offset, int whence );

int dup( int fd );
int dup2( int oldfd, int newfd );

int ftruncate( int fd, off_t length );
int truncate( const char *path, off_t length );

int unlink( const char *path );
int rmdir( const char *path );
int unlinkat( int dirfd, const char *path, int flags );

int link( const char *target, const char *linkpath );
int linkat( int olddirfd, const char *target, int newdirfd, const char *linkpath, int flags );
int symlink( const char *target, const char *linkpath );
int symlinkat( const char *target, int dirfd, const char *linkpath );

ssize_t readlink( const char *path, char *buf, size_t count );
ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count );

int access( const char *path, int mode );
int faccessat( int dirfd, const char *path, int mode, int flags );

int chdir( const char *path );
int fchdir( int dirfd );

int chown( const char *path, uid_t owner, gid_t group );
int fchownat( int fd, const char *path, uid_t owner, gid_t group, int flag );
int fchown( int fildes, uid_t owner, gid_t group );

void _exit( int status );
pid_t getpid( void );
pid_t getppid( void );
pid_t getsid( pid_t pid );
pid_t getpgid( pid_t pid );
pid_t getpgrp( void );
pid_t setsid( void );
int setpgrp( void );
int setpgid( pid_t pid, pid_t pgid );
int issetugid( void );

int fsync( int fd );
int fdatasync( int fd );

void swab( const void *from, void *to, ssize_t n );

int isatty( int fd );
char *ttyname( int fd );
int ttyname_r( int fd, char *buf, size_t count );

void sync( void );
int syncfs( int fd );

unsigned int sleep( unsigned int seconds );
int usleep( useconds_t usec );

// entrypoints for rename & renameat functions
int renameat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath );

char *getcwd(char *buf, size_t size);

pid_t fork( void );

#ifndef _GETOPT_DEFINED_
#define _GETOPT_DEFINED_
int	 getopt(int, char * const *, const char *);

extern   char *optarg;                  /* getopt(3) external variables */
extern   int opterr;
extern   int optind;
extern   int optopt;
extern   int optreset;
#endif

#ifdef __cplusplus
} // extern C
#endif

#endif

