// -*- C++ -*- (c) 2015 Jiří Weiser

#include <bits/confname.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/hostabi.h>

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

#define _SC_CLK_TCK _HOST__SC_CLK_TCK

#ifdef __cplusplus
extern "C" {
#endif

extern char **environ;

typedef __uint32_t useconds_t;

int close( int fd ) __nothrow;
ssize_t read( int fd, void *buf, size_t count ) __nothrow;
ssize_t write( int fd, const void *buf, size_t count ) __nothrow;

ssize_t pread( int fd, void *buf, size_t count, off_t offset ) __nothrow;
ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset ) __nothrow;

int pipe( int pipefd[ 2 ] ) __nothrow;

off_t lseek( int fd, off_t offset, int whence ) __nothrow;

int dup( int fd ) __nothrow;
int dup2( int oldfd, int newfd ) __nothrow;

int ftruncate( int fd, off_t length ) __nothrow;
int truncate( const char *path, off_t length ) __nothrow;

int unlink( const char *path ) __nothrow;
int rmdir( const char *path ) __nothrow;
int unlinkat( int dirfd, const char *path, int flags ) __nothrow;

int link( const char *target, const char *linkpath ) __nothrow;
int linkat( int olddirfd, const char *target, int newdirfd, const char *linkpath, int flags ) __nothrow;
int symlink( const char *target, const char *linkpath ) __nothrow;
int symlinkat( const char *target, int dirfd, const char *linkpath ) __nothrow;

ssize_t readlink( const char *path, char *buf, size_t count ) __nothrow;
ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count ) __nothrow;

int access( const char *path, int mode ) __nothrow;
int faccessat( int dirfd, const char *path, int mode, int flags ) __nothrow;

int chdir( const char *path ) __nothrow;
int fchdir( int dirfd ) __nothrow;

int chown( const char *path, uid_t owner, gid_t group ) __nothrow;
int fchownat( int fd, const char *path, uid_t owner, gid_t group, int flag ) __nothrow;
int fchown( int fildes, uid_t owner, gid_t group ) __nothrow;

void _exit( int status ) __nothrow;
pid_t getpid( void ) __nothrow;
pid_t getppid( void ) __nothrow;
pid_t getsid( pid_t pid ) __nothrow;
pid_t getpgid( pid_t pid ) __nothrow;
pid_t getpgrp( void ) __nothrow;
pid_t setsid( void ) __nothrow;
uid_t geteuid( void ) __nothrow;
gid_t getegid( void ) __nothrow;
int setpgrp( void ) __nothrow;
int setpgid( pid_t pid, pid_t pgid ) __nothrow;
int issetugid( void ) __nothrow;
int getgroups( int gidsetsize, gid_t grouplist[] ) __nothrow;
gid_t getgid( void );
uid_t getuid( void );

int fsync( int fd ) __nothrow;
int __libc_fsync( int fd ) __nothrow;
int fdatasync( int fd ) __nothrow;

void swab( const void *from, void *to, ssize_t n ) __nothrow;

int isatty( int fd ) __nothrow;
char *ttyname( int fd ) __nothrow;
int ttyname_r( int fd, char *buf, size_t count ) __nothrow;

void sync( void ) __nothrow;
int syncfs( int fd ) __nothrow;

unsigned int sleep( unsigned int seconds ) __nothrow;
int usleep( useconds_t usec ) __nothrow;

// entrypoints for rename & renameat functions
int renameat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath ) __nothrow;

char *getcwd(char *buf, size_t size) __nothrow;
int __getcwd(char *buf, size_t size) __nothrow;

pid_t fork( void ) __nothrow;

#ifndef _GETOPT_DEFINED_
#define _GETOPT_DEFINED_
int	 getopt(int, char * const *, const char *) __nothrow;

extern   char *optarg;                  /* getopt(3) external variables */
extern   int opterr;
extern   int optind;
extern   int optopt;
extern   int optreset;
#endif

long sysconf( int ) __nothrow;
int nice( int ) __nothrow;
int gethostname( char *name, size_t namelen ) __nothrow;
unsigned alarm( unsigned ) __nothrow;
char *getlogin( void ) __nothrow;
int getpagesize( void ) __nothrow;

#define _PC_LINK_MAX             _HOST__PC_LINK_MAX
#define _PC_MAX_CANON            _HOST__PC_MAX_CANON
#define _PC_MAX_INPUT            _HOST__PC_MAX_INPUT
#define _PC_NAME_MAX             _HOST__PC_NAME_MAX
#define _PC_PATH_MAX             _HOST__PC_PATH_MAX
#define _PC_PIPE_BUF             _HOST__PC_PIPE_BUF
#define _PC_CHOWN_RESTRICTED     _HOST__PC_CHOWN_RESTRICTED
#define _PC_NO_TRUNC             _HOST__PC_NO_TRUNC
#define _PC_VDISABLE             _HOST__PC_VDISABLE
#define _PC_2_SYMLINKS           _HOST__PC_2_SYMLINKS
#define _PC_ALLOC_SIZE_MIN       _HOST__PC_ALLOC_SIZE_MIN
#define _PC_ASYNC_IO             _HOST__PC_ASYNC_IO
#define _PC_FILESIZEBITS         _HOST__PC_FILESIZEBITS
#define _PC_PRIO_IO              _HOST__PC_PRIO_IO
#define _PC_REC_INCR_XFER_SIZE   _HOST__PC_REC_INCR_XFER_SIZE
#define _PC_REC_MAX_XFER_SIZE    _HOST__PC_REC_MAX_XFER_SIZE
#define _PC_REC_MIN_XFER_SIZE    _HOST__PC_REC_MIN_XFER_SIZE
#define _PC_REC_XFER_ALIGN       _HOST__PC_REC_XFER_ALIGN
#define _PC_SYMLINK_MAX          _HOST__PC_SYMLINK_MAX
#define _PC_SYNC_IO              _HOST__PC_SYNC_IO

long pathconf( const char *, int ) __nothrow;
long fpathconf( int, int ) __nothrow;

#ifdef __cplusplus
} // extern C
#endif

#endif

