// -*- C++ -*- (c) 2015 Jiří Weiser

#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>

#include <bits/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dios.h>

#include <sys/syswrap.h>

#include <dios/sys/memory.hpp>

struct DirWrapper
{
    int fd;
    struct dirent readdir_entry;
    char *readdir_entry_raw() { return reinterpret_cast< char * >( &readdir_entry ); }
};

struct _nofail {};
void *operator new( size_t s, _nofail ) { return __vm_obj_make( s, _VM_PT_Heap ); }
void *operator new[]( size_t s, _nofail ) { return __vm_obj_make( s, _VM_PT_Heap ); }
static _nofail nofail;

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
        struct stat fdStat;

        int result = __libc_fstat( fd, &fdStat );
        if ( result == -1 )
            return nullptr;

        mode_t fdMode( fdStat.st_mode );

        if( ( fdMode & S_IFMT ) != S_IFDIR )
        {
            errno = ENOTDIR;
            return nullptr;
        }

        int newFD = __libc_dup( fd );
        if ( newFD > 0 ) {
            DirWrapper *wrapper = new ( nofail ) DirWrapper;
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
        if ( fd >= 0 ) {
            DirWrapper *wrapper = new ( nofail ) DirWrapper;
            wrapper->fd = fd;
            return wrapper;
        } else {
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
        __vm_obj_free( wrapper );
        return __libc_close( fd );
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

        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );

        int res = __libc_read( wrapper->fd, wrapper->readdir_entry_raw(), sizeof( struct dirent ) );
        return res > 0 ? &wrapper->readdir_entry : nullptr;
    }

    int readdir_r( DIR *dirp, struct dirent *entry, struct dirent **result )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        char *dirInfo = reinterpret_cast< char * >(entry);

        int res = __libc_read(wrapper->fd, dirInfo, sizeof( struct dirent ));
        *result = ( res == sizeof( struct dirent ) ) ? entry : nullptr;
        return (res >= 0) ? 0 : -1;
    }

    void rewinddir( DIR *dirp )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >(dirp);
        __libc_lseek( wrapper->fd, 0, SEEK_SET );
    }

    int scandir( const char *path, struct dirent ***namelist,
              int ( *filter )( const struct dirent * ),
              int ( *compare )( const struct dirent **, const struct dirent ** ) )
    {

        int length = 0;
        DIR *dirp = opendir( path );

        if( !dirp ){
            return -1;
        }

        struct dirent **entries = nullptr;
        struct dirent *workingEntry = new ( nofail ) struct dirent;

        while ( true ) {
            struct dirent* ent = readdir( dirp );
            if ( !ent )
                break;

            workingEntry->d_ino = ent->d_ino;
            char *x = std::copy( ent->d_name, ent->d_name + strlen(ent->d_name), workingEntry->d_name );
            *x = '\0';
            if ( filter && !filter( workingEntry ) )
                continue;

            struct dirent **newEntries = new ( nofail ) struct dirent *[ length + 1 ];
            if ( length )
                std::memcpy( newEntries, entries, length * sizeof( struct dirent * ) );
            std::swap( entries, newEntries );
            if ( newEntries )
                __vm_obj_free( newEntries );
            entries[ length ] = workingEntry;
            workingEntry = new ( nofail ) struct dirent;
            ++length;
        }
        __vm_obj_free( workingEntry );
        closedir( dirp );

        typedef int( *cmp )( const void *, const void * );
        std::qsort( entries, length, sizeof( struct dirent * ), reinterpret_cast< cmp >( compare ) );

        *namelist = entries;
        return length;
    }

    long telldir( DIR *dirp )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        return __libc_lseek( wrapper->fd, 0, SEEK_CUR );
    }

    void seekdir( DIR *dirp, long loc )
    {
        DirWrapper *wrapper = reinterpret_cast< DirWrapper * >( dirp );
        __libc_lseek( wrapper->fd, loc, SEEK_SET );
    }


    ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    {
        return __libc_sendto(sockfd, buf, len, flags, NULL, 0);
    }

    ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
    {
       int currPos = __libc_lseek( fd, 0, SEEK_CUR );

        int moved = __libc_lseek( fd, offset, SEEK_SET );
        if ( moved != offset ) 
            return -1;

        int writed = __libc_write( fd, buf, count );
        __libc_lseek( fd, currPos, SEEK_SET );

        return writed;
    }

    ssize_t pread(int fd, void *buf, size_t count, off_t offset)
    {
        int currPos = __libc_lseek( fd, 0, SEEK_CUR );

        int moved = __libc_lseek( fd, offset, SEEK_SET );
        if ( moved != offset ) 
            return -1;

        int readed = __libc_read( fd, buf, count );
        __libc_lseek( fd, currPos, SEEK_SET );

        return readed;
    }

    int mkfifoat(int dirfd, const char *pathname, mode_t mode)
    {
        return __libc_mknodat( dirfd, pathname, ( ACCESSPERMS & mode ) | S_IFIFO, 0 );
        
    }

    int mkfifo(const char *pathname, mode_t mode)
    {
        return __libc_mknod( pathname, ( ACCESSPERMS & mode ) | S_IFIFO, 0 );
    }


    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        return __libc_recvfrom( sockfd, buf, len, flags, nullptr, nullptr );
    }

    char *ttyname(int fd)
    {
        struct stat fdStat;

        //just to set errno if fd is not valid file descriptor
        __libc_fstat( fd, &fdStat );
        return nullptr;
    }

    int ttyname_r(int fd, char *, size_t )
    {
        struct stat fdStat;

        //just to set errno if fd is not valid file descriptor
        int res = __libc_fstat( fd, &fdStat );
        return res == 0 ? ENOTTY : res;
    }

    int isatty(int fd)
    {
        struct stat fdStat;

        //just to set errno if fd is not valid file descriptor
        int res = __libc_fstat( fd, &fdStat );
        return res == 0 ? EINVAL : res;
    }


    #if defined(__divine__)
    int alphasort( const struct dirent **a, const struct dirent **b ) {
        return std::strcoll( (*a)->d_name, (*b)->d_name );
    }

    #endif

}

