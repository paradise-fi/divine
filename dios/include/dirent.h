// -*- C++ -*- (c) 2015 Jiří Weiser
#include <sys/types.h>

#ifndef _DIRENT_H
#define _DIRENT_H

#define _HOST_dirent dirent
#include <sys/hostabi.h>
#undef _HOST_dirent

#ifdef __cplusplus
extern "C" {
#endif

typedef void DIR;

#define FS_NOINLINE __attribute__((noinline))

int alphasort( const struct dirent **, const struct dirent ** );

FS_NOINLINE int closedir( DIR *dirp );
FS_NOINLINE int dirfd( DIR *dirp );
FS_NOINLINE DIR *fdopendir( int fd );
FS_NOINLINE DIR *opendir( const char *path );
FS_NOINLINE struct dirent *readdir( DIR *dirp );
FS_NOINLINE int readdir_r( DIR *dirp, struct dirent *entry, struct dirent **result );
FS_NOINLINE void rewinddir( DIR * );
FS_NOINLINE int scandir( const char *, struct dirent ***,
                         int (*filter)( const struct dirent * ),
                         int (*compare)( const struct dirent **, const struct dirent ** ));
FS_NOINLINE void seekdir( DIR *, long );
FS_NOINLINE long telldir( DIR * );

#ifdef __cplusplus
}
#endif

#undef FS_NOINLINE

#endif
