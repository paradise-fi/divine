#ifndef _FCNTL_H_
#define _FCNTL_H_

#define AT_FDCWD            -100

#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EACCESS          0x200

#define O_RDONLY            0
#define O_WRONLY            1
#define O_RDWR              2
#define O_CREAT            64
#define O_EXCL            128
#define O_APPEND         1024
#define O_TRUNC           512
#define O_NOFOLLOW     131072



#ifdef __cplusplus
extern "C" {
#endif

#define FS_NOINLINE __attribute__((noinline))

FS_NOINLINE int creat( const char *path, int mode );
FS_NOINLINE int open( const char *path, int flags, ... );
FS_NOINLINE int openat( int dirfd, const char *path, int flags, ... );

#undef FS_NOINLINE

#ifdef __cplusplus
}
#endif


#endif
