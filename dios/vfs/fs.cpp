// -*- C++ -*- (c) 2015 Jiří Weiser

#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>

#include <bits/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dios.h>

#include "manager.h"

struct DirWrapper {
    int fd;
};

using __dios::fs::Error;

extern "C" {

    void swab( const void *_from, void *_to, ssize_t n ) {
        const char *from = reinterpret_cast< const char * >( _from );
        char *to = reinterpret_cast< char * >( _to );
        for ( ssize_t i = 0; i < n / 2; ++i ) {
            *to = *( from + 1 );
            *( to + 1 ) = *from;
            to += 2;
            from += 2;
        }
    }

    DIR *fdopendir( int fd )
    {
        using __dios::fs::Mode;
        struct stat fdStat;

        int result = fstat( fd, &fdStat );
        if ( result == -1 )
            return nullptr;

        Mode fdMode( fdStat.st_mode );
        if( !fdMode.isDirectory( ) ) {
            errno = ENOTDIR;
            return nullptr;
        }

        int newFD = dup( fd );
        if ( newFD > 0 ) {
            DirWrapper *wrapper = reinterpret_cast< DirWrapper* >( FS_MALLOC( sizeof( struct DirWrapper ) ) );
            wrapper->fd = newFD;
            return wrapper;
        }else {
            errno = EBADF;
            return nullptr;
        }

    }

    DIR *opendir( const char *name )
    {
        int fd = open( name, O_DIRECTORY );
        if ( fd > 0 ) {
            DirWrapper *wrapper = reinterpret_cast< DirWrapper* >( FS_MALLOC( sizeof( struct DirWrapper ) ) );
            wrapper->fd = fd;
            return wrapper;
        }else {
            return nullptr;
        }
    }

    int closedir(DIR *dirp)
    {
        if ( dirp == nullptr )
        {
            errno = EBADF;
            return -1;
        }

        DirWrapper *wrapper = reinterpret_cast< DirWrapper* >( dirp );
        int fd = wrapper->fd;
        FS_FREE( wrapper );
        return close( fd );
    }

    int dirfd( DIR *dirp )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper* >( dirp );
        return wrapper->fd;
    }

    struct dirent *readdir( DIR *dirp )
    {
        if ( !dirp ) {
            errno = EBADF;
            return nullptr;
        }

        struct dirent* retval = reinterpret_cast< struct dirent * >( FS_MALLOC( sizeof( struct dirent )));
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        char *dirInfo = reinterpret_cast< char * >( retval );

        int res = read( wrapper->fd, dirInfo, sizeof( struct dirent ) );
        return ( res > 0 ) ? retval : nullptr;
    }

    int readdir_r( DIR *dirp, struct dirent *entry, struct dirent **result )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        char *dirInfo = reinterpret_cast< char * >(entry);

        int res = read(wrapper->fd, dirInfo, sizeof( struct dirent ));
        *result = ( res == sizeof( struct dirent ) ) ? entry : nullptr;
        return (res >= 0) ? 0 : -1;
    }

    void rewinddir( DIR *dirp )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >(dirp);
        lseek( wrapper->fd, 0, SEEK_SET );
    }

    int scandir( const char *path, struct dirent ***namelist,
              int ( *filter )( const struct dirent * ),
              int ( *compare )( const struct dirent **, const struct dirent ** ) )
    {

        using namespace __dios::fs::flags;
        int length = 0;
        DIR *dirp = opendir( path );

        if( !dirp ){
            return -1;
        }

        struct dirent **entries = nullptr;
        struct dirent *workingEntry = static_cast< struct dirent * >( FS_MALLOC( sizeof( struct dirent ) ) );

        while ( true ) {
            struct dirent* ent = readdir( dirp );
            if ( !ent )
                break;

            workingEntry->d_ino = ent->d_ino;
            char *x = std::copy( ent->d_name, ent->d_name + strlen(ent->d_name), workingEntry->d_name );
            *x = '\0';
            if ( filter && !filter( workingEntry ) )
                continue;

            struct dirent **newEntries = static_cast< struct dirent ** >( FS_MALLOC( ( length + 1 ) * sizeof( struct dirent * ) ) );
            if ( length )
                std::memcpy( newEntries, entries, length * sizeof( struct dirent * ) );
            std::swap( entries, newEntries );
            if ( newEntries )
                FS_FREE( newEntries );
            entries[ length ] = workingEntry;
            workingEntry = static_cast< struct dirent * >( FS_MALLOC( sizeof( struct dirent ) ) );
            ++length;
        }
        FS_FREE( workingEntry );
        closedir( dirp );

        typedef int( *cmp )( const void *, const void * );
        std::qsort( entries, length, sizeof( struct dirent * ), reinterpret_cast< cmp >( compare ) );

        *namelist = entries;
        return length;
    }

    long telldir( DIR *dirp )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        return lseek( wrapper->fd, 0, SEEK_CUR );
    }

    void seekdir( DIR *dirp, long loc )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        lseek( wrapper->fd, loc, SEEK_SET );
    }


    ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    {
        return sendto(sockfd, buf, len, flags, NULL, 0);
    }

    ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
    {
       int currPos = lseek( fd, 0, SEEK_CUR );

        int moved = lseek( fd, offset, SEEK_SET );
        if ( moved != offset ) 
            return -1;

        int writed = write( fd, buf, count );
        lseek( fd, currPos, SEEK_SET );

        return writed;
    }

    ssize_t pread(int fd, void *buf, size_t count, off_t offset)
    {
        int currPos = lseek( fd, 0, SEEK_CUR );

        int moved = lseek( fd, offset, SEEK_SET );
        if ( moved != offset ) 
            return -1;

        int readed = read( fd, buf, count );
        lseek( fd, currPos, SEEK_SET );

        return readed;
    }

    int mkfifoat(int dirfd, const char *pathname, mode_t mode)
    {
        return mknodat( dirfd, pathname, ( ACCESSPERMS & mode ) | S_IFIFO, 0 );
        
    }

    int mkfifo(const char *pathname, mode_t mode)
    {
        return mknod( pathname, ( ACCESSPERMS & mode ) | S_IFIFO, 0 );
    }


    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        return recvfrom( sockfd, buf, len, flags, nullptr, nullptr );   
    }

    char *ttyname(int fd)
    {
        using __dios::fs::Mode;
        struct stat fdStat;

        //just to set errno if fd is not valid file descriptor
        fstat( fd, &fdStat );
        return nullptr;
    }

    int ttyname_r(int fd, char *, size_t )
    {
        using __dios::fs::Mode;
        struct stat fdStat;

        //just to set errno if fd is not valid file descriptor
        int res = fstat( fd, &fdStat );
        return res == 0 ? ENOTTY : res;
    }

    int isatty(int fd)
    {
        using __dios::fs::Mode;
        struct stat fdStat;

        //just to set errno if fd is not valid file descriptor
        int res = fstat( fd, &fdStat );
        return res == 0 ? EINVAL : res;
    }


    #if defined(__divine__)
    int alphasort( const struct dirent **a, const struct dirent **b ) {
        return std::strcoll( (*a)->d_name, (*b)->d_name );
    }

    #endif

}

