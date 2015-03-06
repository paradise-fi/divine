#include <iostream>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include <bits/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include "fs-manager.h"

#ifdef __divine__
# define FS_MASK __divine_interrupt_mask();
# define FS_MALLOC( x ) __divine_malloc( x )
# define FS_PROBLEM( msg ) __divine_problem( 1, msg )
#else
# define FS_MASK
# define FS_MALLOC( x ) std::malloc( x )
# define FS_PROBLEM( msg )
#endif

using divine::fs::Error;
using divine::fs::vfs;

extern "C" {

static void _initStat( struct stat *buf ) {
    buf->st_dev = 0;
    buf->st_rdev = 0;
    buf->st_atime = 0;
    buf->st_mtime = 0;
    buf->st_ctime = 0;
}

static int _fillStat( const divine::fs::Node item, struct stat *buf ) {
    if ( !item )
        return -1;
    _initStat( buf );
    buf->st_ino = item->ino();
    buf->st_mode = item->mode();
    buf->st_nlink = item.use_count();
    buf->st_size = item->size();
    buf->st_uid = item->uid();
    buf->st_gid = item->gid();
    buf->st_blksize = 512;
    buf->st_blocks = ( buf->st_size + 1 ) / buf->st_blksize;
    return 0;
}

int openat( int dirfd, const char *path, int flags, ... ) {
    FS_MASK
    using namespace divine::fs::flags;
    divine::fs::Flags< Open > f = Open::NoFlags;
    mode_t m = 0;
    // special behaviour - check for access rights but do not grant them
    if ( ( flags & 3 ) == 3)
        f |= Open::NoAccess;
    if ( flags & O_RDWR ) {
        f |= Open::Read;
        f |= Open::Write;
    }
    else if ( flags & O_WRONLY )
        f |= Open::Write;
    else
        f |= Open::Read;

    if ( flags & O_CREAT ) {
        f |= divine::fs::flags::Open::Create;
        va_list args;
        va_start( args, flags );
        m = unsigned( va_arg( args, int ) );
        va_end( args );
    }

    if ( flags & O_EXCL )
        f |= divine::fs::flags::Open::Excl;
    if ( flags & O_TRUNC )
        f |= divine::fs::flags::Open::Truncate;
    if ( flags & O_APPEND )
        f |= divine::fs::flags::Open::Append;
    if ( flags & O_NOFOLLOW )
        f |= divine::fs::flags::Open::SymNofollow;

    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        return vfs.instance().openFileAt( dirfd, path, f, m );
    } catch ( Error & ) {
        return -1;
    }
}
int open( const char *path, int flags, ... ) {
    FS_MASK
    int mode = 0;
    if ( flags & O_CREAT ) {
        va_list args;
        va_start( args, flags );
        mode = va_arg( args, int );
        va_end( args );
    }
    return openat( AT_FDCWD, path, flags, mode );
}
int creat( const char *path, mode_t mode ) {
    FS_MASK
    return openat( AT_FDCWD, path, O_CREAT | O_WRONLY | O_TRUNC, mode );
}

int close( int fd ) {
    FS_MASK
    try {
        vfs.instance().closeFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

ssize_t write( int fd, const void *buf, size_t count ) {
    FS_MASK
    try {
        auto f = vfs.instance().getFile( fd );
        return f->write( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}
ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset ) {
    FS_MASK
    try {
        auto f = vfs.instance().getFile( fd );
        size_t savedOffset = f->offset();
        f->offset( offset );
        auto d = divine::fs::utils::make_defer( [&]{ f->offset( savedOffset ); } );
        return f->write( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}

ssize_t read( int fd, void *buf, size_t count ) {
    FS_MASK
    try {
        auto f = vfs.instance().getFile( fd );
        return f->read( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}
ssize_t pread( int fd, void *buf, size_t count, off_t offset ) {
    FS_MASK
    try {
        auto f = vfs.instance().getFile( fd );
        size_t savedOffset = f->offset();
        f->offset( offset );
        auto d = divine::fs::utils::make_defer( [&]{ f->offset( savedOffset ); } );
        return f->read( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}
int mkdirat( int dirfd, const char *path, mode_t mode ) {
    FS_MASK
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        vfs.instance().createDirectoryAt( dirfd, path, mode );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int mkdir( const char *path, mode_t mode ) {
    FS_MASK
    return mkdirat( AT_FDCWD, path, mode );
}

int mkfifoat( int dirfd, const char *path, mode_t mode ) {
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        vfs.instance().createFifoAt( dirfd, path, mode );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int mkfifo( const char *path, mode_t mode ) {
    FS_MASK
    return mkfifoat( AT_FDCWD, path, mode );
}

int unlink( const char *path ) {
    FS_MASK
    try {
        vfs.instance().removeFile( path );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int rmdir( const char *path ) {
    FS_MASK
    try {
        vfs.instance().removeDirectory( path );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int unlinkat( int dirfd, const char *path, int flags ) {
    FS_MASK
    divine::fs::flags::At f;
    switch( flags ) {
    case 0:
        f = divine::fs::flags::At::NoFlags;
        break;
    case AT_REMOVEDIR:
        f = divine::fs::flags::At::RemoveDir;
        break;
    default:
        f = divine::fs::flags::At::Invalid;
        break;
    }
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        vfs.instance().removeAt( dirfd, path, f );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

off_t lseek( int fd, off_t offset, int whence ) {
    FS_MASK
    try {
        divine::fs::Seek w = divine::fs::Seek::Undefined;
        switch( whence ) {
        case SEEK_SET:
            w = divine::fs::Seek::Set;
            break;
        case SEEK_CUR:
            w = divine::fs::Seek::Current;
            break;
        case SEEK_END:
            w = divine::fs::Seek::End;
            break;
        }
        return vfs.instance().lseek( fd, offset, w );
    } catch ( Error & ) {
        return -1;
    }
}
int dup( int fd ) {
    FS_MASK
    try {
        return vfs.instance().duplicate( fd );
    } catch ( Error & ) {
        return -1;
    }
}
int dup2( int oldfd, int newfd ) {
    FS_MASK
    try {
        return vfs.instance().duplicate2( oldfd, newfd );
    } catch ( Error & ) {
        return -1;
    }
}
int symlinkat( const char *target, int dirfd, const char *linkpath ) {
    FS_MASK
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        vfs.instance().createSymLinkAt( dirfd, linkpath, target );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int symlink( const char *target, const char *linkpath ) {
    FS_MASK
    return symlinkat( target, AT_FDCWD, linkpath );
}
int linkat( int olddirfd, const char *target, int newdirfd, const char *linkpath, int flags ) {
    FS_MASK
    if ( olddirfd == AT_FDCWD )
        olddirfd = divine::fs::CURRENT_DIRECTORY;
    if ( newdirfd == AT_FDCWD )
        newdirfd = divine::fs::CURRENT_DIRECTORY;

    divine::fs::Flags< divine::fs::flags::At > fl = divine::fs::flags::At::NoFlags;
    if ( flags & AT_SYMLINK_FOLLOW )   fl |= divine::fs::flags::At::SymFollow;
    if ( ( flags | AT_SYMLINK_FOLLOW ) != AT_SYMLINK_FOLLOW )
        fl |= divine::fs::flags::At::Invalid;

    try {
        vfs.instance().createHardLinkAt( newdirfd, linkpath, olddirfd, target, fl );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int link( const char *target, const char *linkpath ) {
    FS_MASK
    return linkat( AT_FDCWD, target, AT_FDCWD, linkpath, 0 );
}

ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count ) {
    FS_MASK
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        return vfs.instance().readLinkAt( dirfd, path, buf, count );
    } catch ( Error & ) {
        return -1;
    }
}
ssize_t readlink( const char *path, char *buf, size_t count ) {
    FS_MASK
    return readlinkat( AT_FDCWD, path, buf, count );
}
int faccessat( int dirfd, const char *path, int mode, int flags ) {
    FS_MASK
    divine::fs::Flags< divine::fs::flags::Access > m = divine::fs::flags::Access::OK;
    if ( mode & R_OK )  m |= divine::fs::flags::Access::Read;
    if ( mode & W_OK )  m |= divine::fs::flags::Access::Write;
    if ( mode & X_OK )  m |= divine::fs::flags::Access::Execute;
    if ( ( mode | R_OK | W_OK | X_OK ) != ( R_OK | W_OK | X_OK ) )
        m |= divine::fs::flags::Access::Invalid;

    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;

    divine::fs::Flags< divine::fs::flags::At > fl = divine::fs::flags::At::NoFlags;
    if ( flags & AT_EACCESS )   fl |= divine::fs::flags::At::EffectiveID;
    if ( flags & AT_SYMLINK_NOFOLLOW )  fl |= divine::fs::flags::At::SymNofollow;
    if ( ( flags | AT_EACCESS | AT_SYMLINK_NOFOLLOW ) != ( AT_EACCESS | AT_SYMLINK_NOFOLLOW ) )
        fl |= divine::fs::flags::At::Invalid;

    try {
        vfs.instance().accessAt( dirfd, path, m, fl );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int access( const char *path, int mode ) {
    FS_MASK
    return faccessat( AT_FDCWD, path, mode, 0 );
}

int stat( const char *path, struct stat *buf ) {
    FS_MASK
    try {
        auto item = vfs.instance().findDirectoryItem( path );
        if ( !item )
            throw Error( ENOENT );
        return _fillStat( item, buf );
    } catch ( Error & ) {
        return -1;
    }
}

int lstat( const char *path, struct stat *buf ) {
    FS_MASK
    try {
        auto item = vfs.instance().findDirectoryItem( path, false );
        if ( !item )
            throw Error( ENOENT );
        return _fillStat( item, buf );
    } catch ( Error & ) {
        return -1;
    }
}

int fstat( int fd, struct stat *buf ) {
    FS_MASK
    try {
        auto item = vfs.instance().getFile( fd );
        return _fillStat( item->inode(), buf );
    } catch ( Error & ) {
        return -1;
    }
}

mode_t umask( mode_t mask ) {
    FS_MASK
    mode_t result = vfs.instance().umask();
    vfs.instance().umask( mask & 0777 );
    return result;
}

int chdir( const char *path ) {
    FS_MASK
    try {
        vfs.instance().changeDirectory( path );
        return 0;
    } catch( Error & ) {
        return -1;
    }
}

int fchdir( int dirfd ) {
    FS_MASK
    try {
        vfs.instance().changeDirectory( dirfd );
        return 0;
    } catch( Error & ) {
        return -1;
    }
}

int fdatasync( int fd ) {
    FS_MASK
    try {
        vfs.instance().getFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int fsync( int fd ) {
    FS_MASK
    try {
        vfs.instance().getFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int ftruncate( int fd, off_t length ) {
    FS_MASK
    try {
        auto item = vfs.instance().getFile( fd );
        if ( !item->flags().has( divine::fs::flags::Open::Write ) )
            throw Error( EINVAL );
        vfs.instance().truncate( item->inode(), length );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int truncate( const char *path, off_t length ) {
    FS_MASK
    try {
        auto item = vfs.instance().findDirectoryItem( path );
        vfs.instance().truncate( item, length );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

void swab( const void *_from, void *_to, ssize_t n ) {
    FS_MASK
    const char *from = reinterpret_cast< const char * >( _from );
    char *to = reinterpret_cast< char * >( _to );
    for ( ssize_t i = 0; i < n/2; ++i ) {
        *to = *(from + 1);
        *(to + 1) = *from;
        to += 2;
        from += 2;
    }
}

int isatty( int fd ) {
    FS_MASK
    try {
        vfs.instance().getFile( fd );
        errno = EINVAL;
    } catch ( Error & ) {
    }
    return 0;
}
char *ttyname( int fd ) {
    FS_MASK
    try {
        vfs.instance().getFile( fd );
        errno = ENOTTY;
    } catch ( Error & ) {
    }
    return nullptr;
}
int ttyname_r( int fd, char *buf, size_t count ) {
    FS_MASK
    try {
        vfs.instance().getFile( fd );
        return ENOTTY;
    } catch ( Error &e ) {
        return e.code();
    }
}

void sync( void ) {}
int syncfs( int fd ) {
    FS_MASK
    try {
        vfs.instance().getFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}


int _FS_renameitemat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath ) {
    FS_MASK
    if ( olddirfd == AT_FDCWD )
        olddirfd = divine::fs::CURRENT_DIRECTORY;
    if ( newdirfd == AT_FDCWD )
        newdirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        vfs.instance().renameAt( newdirfd, newpath, olddirfd, oldpath );
        return 0;
    } catch ( Error & ) {
        return -1;
    }

}
int _FS_renameitem( const char *oldpath, const char *newpath ) {
    FS_MASK
    return _FS_renameitemat( AT_FDCWD, oldpath, AT_FDCWD, newpath );
}

int pipe( int pipefd[ 2 ] ) {
    FS_MASK
    try {
        std::tie( pipefd[ 0 ], pipefd[ 1 ] ) = vfs.instance().pipe();
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int fchmodeat( int dirfd, const char *path, mode_t mode, int flags ) {
    FS_MASK

    divine::fs::Flags< divine::fs::flags::At > fl = divine::fs::flags::At::NoFlags;
    if ( flags & AT_SYMLINK_NOFOLLOW )  fl |= divine::fs::flags::At::SymNofollow;
    if ( ( flags | AT_SYMLINK_NOFOLLOW ) != AT_SYMLINK_NOFOLLOW )
        fl |= divine::fs::flags::At::Invalid;

    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        vfs.instance().chmodAt( dirfd, path, mode, fl );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int chmod( const char *path, mode_t mode ) {
    FS_MASK
    return fchmodeat( AT_FDCWD, path, mode, 0 );
}
int fchmod( int fd, mode_t mode ) {
    FS_MASK
    try {
        vfs.instance().chmod( fd, mode );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int alphasort( const struct dirent **a, const struct dirent **b ) {
    return std::strcoll( (*a)->d_name, (*b)->d_name );
}

int closedir( DIR *dirp ) {
    FS_MASK
    try {
        vfs.instance().closeDirectory( dirp );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int dirfd( DIR *dirp ) {
    FS_MASK
    try {
        return vfs.instance().getDirectory( dirp )->fd();
    } catch ( Error & ) {
        return -1;
    }
}

DIR *fdopendir( int fd ) {
    FS_MASK
    try {
        return vfs.instance().openDirectory( fd );
    } catch ( Error & ) {
        return nullptr;
    }
}

DIR *opendir( const char *path ) {
    FS_MASK
    using namespace divine::fs::flags;
    divine::fs::Flags< Open > f = Open::Read;
    try {
        int fd = vfs.instance().openFileAt( divine::fs::CURRENT_DIRECTORY, path, f, 0 );
        return vfs.instance().openDirectory( fd );
    } catch ( Error & ) {
        return nullptr;
    }
}

struct dirent *readdir( DIR *dirp ) {
    FS_MASK
    static struct dirent entry;

    try {
        auto dir = vfs.instance().getDirectory( dirp );
        auto ent = dir->get();
        if ( !ent )
            return nullptr;
        entry.d_ino = ent->ino();
        std::copy( ent->name().begin(), ent->name().end(), entry.d_name ) = '\0';
        dir->next();
        return &entry;
    } catch ( Error & ) {
        return nullptr;
    }
}

int readdir_r( DIR *dirp, struct dirent *entry, struct dirent **result ) {
    FS_MASK

    try {
        auto dir = vfs.instance().getDirectory( dirp );
        auto ent = dir->get();
        if ( ent ) {
            entry->d_ino = ent->ino();
            std::copy( ent->name().begin(), ent->name().end(), entry->d_name ) = '\0';
            *result = entry;
            dir->next();
        }
        else
            *result = nullptr;
        return 0;
    } catch ( Error &e ) {
        return e.code();
    }
}

void rewinddir( DIR *dirp ) {
    FS_MASK
    e = errno;
    try {
        vfs.instance().getDirectory( dirp )->rewind();
    } catch ( Error & ) {
        errno = e;
    }
}

int scandir( const char *path, struct dirent ***namelist,
             int (*filter)( const struct dirent * ),
             int (*compare)( const struct dirent **, const struct dirent ** ))
{
    FS_MASK
    using namespace divine::fs::flags;
    divine::fs::Flags< Open > f = Open::Read;
    DIR *dirp = nullptr;
    try {
        int length = 0;
        int fd = vfs.instance().openFileAt( divine::fs::CURRENT_DIRECTORY, path, f, 0 );
        dirp = vfs.instance().openDirectory( fd );

        struct dirent **entries = nullptr;
        struct dirent *workingEntry = (struct dirent *)FS_MALLOC( sizeof( struct dirent ) );

        //divine::fs::DirectoryDescriptor dir;
        //const divine::fs::DirectoryItemLabel *ent;

        while ( true ) {
            auto dir = vfs.instance().getDirectory( dirp );
            auto ent = dir->get();
            if ( !ent )
                break;

            workingEntry->d_ino = ent->ino();
            std::copy( ent->name().begin(), ent->name().end(), workingEntry->d_name ) = '\0';
            dir->next();

            if ( filter && !filter( workingEntry ) )
                continue;

            struct dirent **newEntries = FS_MALLOC( ( length + 1 ) * sizeof( struct dirent * ) );
            if ( length )
                std::memcpy( newEntries, entries, length * sizeof( struct dirent * ) );
            std::swap( entries, newEntries );
            std::free( newEntries );
            entries[ length ] = workingEntry;
            workingEntry = FS_MALLOC( sizeof( struct dirent ) );
            ++length;
        }

        vfs.instance().closeDirectory( dirp );
        typedef int( *cmp )( const void *, const void * );
        std::qsort( entries, length, sizeof( struct dirent * ), reinterpret_cast< cmp >( compare ) );
        return 0;
    } catch ( Error & ) {
        if ( dirp )
            vfs.instance().closeDirectory( dirp );
        return -1;
    }
}

long telldir( DIR *dirp ) {
    FS_MASK
    e = errno;
    try {
        return vfs.instance().getDirectory( dirp )->tell();
    } catch ( Error & ) {
        return -1;
    }
}
void seekdir( DIR *dirp, long offset ) {
    FS_MASK
    e = errno;
    try {
        vfs.instance().getDirectory( dirp )->seek( offset );
    } catch ( Error & ) {
        errno = e;
    }
}

} // extern "C"
