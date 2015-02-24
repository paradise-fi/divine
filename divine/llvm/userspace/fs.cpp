#include <iostream>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring.h>

#include <bits/types.h>

#include "fs.h"
#include "fcntl.h"
#include "fs-manager.h"
#include "fs-stat.h"
#include "dirent.h"

#ifdef __divine__
#define FS_MASK __divine_interrupt_mask();
#define FS_MALLOC( x ) __divine_malloc( x )
#else
#define FS_MASK
#define FS_MALLOC( x ) std::malloc( x )
#endif

namespace divine {
namespace fs {
extern FSManager filesystem;
} // namespace fs
} // namespace divine

using divine::fs::Error;

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
    unsigned m = 0;
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
        return divine::fs::filesystem.openFileAt( dirfd, path, f, m );
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
int creat( const char *path, int mode ) {
    FS_MASK
    return openat( AT_FDCWD, path, O_CREAT | O_WRONLY | O_TRUNC, mode );
}

int close( int fd ) {
    FS_MASK
    try {
        divine::fs::filesystem.closeFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

ssize_t write( int fd, const void *buf, size_t count ) {
    FS_MASK
    try {
        auto f = divine::fs::filesystem.getFile( fd );
        return f->write( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}
ssize_t pwrite( int fd, const void *buf, size_t count, off_t offset ) {
    FS_MASK
    try {
        auto f = divine::fs::filesystem.getFile( fd );
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
        auto f = divine::fs::filesystem.getFile( fd );
        return f->read( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}
ssize_t pread( int fd, void *buf, size_t count, off_t offset ) {
    FS_MASK
    try {
        auto f = divine::fs::filesystem.getFile( fd );
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
        divine::fs::filesystem.createDirectoryAt( dirfd, path, mode );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int mkdir( const char *path, mode_t mode ) {
    FS_MASK
    return mkdirat( AT_FDCWD, path, mode );
}

int unlink( const char *path ) {
    FS_MASK
    try {
        divine::fs::filesystem.removeFile( path );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int rmdir( const char *path ) {
    FS_MASK
    try {
        divine::fs::filesystem.removeDirectory( path );
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
        divine::fs::filesystem.removeAt( dirfd, path, f );
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
        return divine::fs::filesystem.lseek( fd, offset, w );
    } catch ( Error & ) {
        return -1;
    }
}
int dup( int fd ) {
    FS_MASK
    try {
        return divine::fs::filesystem.duplicate( fd );
    } catch ( Error & ) {
        return -1;
    }
}
int dup2( int oldfd, int newfd ) {
    FS_MASK
    try {
        return divine::fs::filesystem.duplicate2( oldfd, newfd );
    } catch ( Error & ) {
        return -1;
    }
}
int symlinkat( const char *target, int dirfd, const char *linkpath ) {
    FS_MASK
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        divine::fs::filesystem.createSymLinkAt( dirfd, linkpath, target );
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
        divine::fs::filesystem.createHardLinkAt( newdirfd, linkpath, olddirfd, target, fl );
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
        return divine::fs::filesystem.readLinkAt( dirfd, path, buf, count );
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
        divine::fs::filesystem.accessAt( dirfd, path, m, fl );
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
        auto item = divine::fs::filesystem.findDirectoryItem( path );
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
        auto item = divine::fs::filesystem.findDirectoryItem( path, false );
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
        auto item = divine::fs::filesystem.getFile( fd );
        return _fillStat( item->inode(), buf );
    } catch ( Error & ) {
        return -1;
    }
}

mode_t umask( mode_t mask ) {
    FS_MASK
    unsigned result = divine::fs::filesystem.umask();
    divine::fs::filesystem.umask( mask & 0777 );
    return result;
}

int chdir( const char *path ) {
    FS_MASK
    try {
        divine::fs::filesystem.changeDirectory( path );
        return 0;
    } catch( Error & ) {
        return -1;
    }
}

int fchdir( int dirfd ) {
    FS_MASK
    try {
        divine::fs::filesystem.changeDirectory( dirfd );
        return 0;
    } catch( Error & ) {
        return -1;
    }
}

int fdatasync( int fd ) {
    FS_MASK
    try {
        divine::fs::filesystem.getFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int fsync( int fd ) {
    FS_MASK
    try {
        divine::fs::filesystem.getFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int ftruncate( int fd, off_t length ) {
    FS_MASK
    try {
        auto item = divine::fs::filesystem.getFile( fd );
        divine::fs::filesystem.truncate( item->inode(), length );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int truncate( const char *path, off_t length ) {
    FS_MASK
    try {
        auto item = divine::fs::filesystem.findDirectoryItem( path );
        divine::fs::filesystem.truncate( item, length );
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
        divine::fs::filesystem.getFile( fd );
        errno = EINVAL;
    } catch ( Error & ) {
    }
    return 0;
}
char *ttyname( int fd ) {
    FS_MASK
    try {
        divine::fs::filesystem.getFile( fd );
        errno = ENOTTY;
    } catch ( Error & ) {
    }
    return nullptr;
}
int ttyname_r( int fd, char *buf, size_t count ) {
    FS_MASK
    try {
        divine::fs::filesystem.getFile( fd );
        return ENOTTY;
    } catch ( Error &e ) {
        return e.code();
    }
}

void sync( void ) {}
int syncfs( int fd ) {
    FS_MASK
    try {
        divine::fs::filesystem.getFile( fd );
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
        divine::fs::filesystem.renameAt( newdirfd, newpath, olddirfd, oldpath );
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
        std::tie( pipefd[ 0 ], pipefd[ 1 ] ) = divine::fs::filesystem.pipe();
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int fchmodeat( int dirfd, const char *path, int mode, int flags ) {
    FS_MASK

    divine::fs::Flags< divine::fs::flags::At > fl = divine::fs::flags::At::NoFlags;
    if ( flags & AT_SYMLINK_NOFOLLOW )  fl |= divine::fs::flags::At::SymNofollow;
    if ( ( flags | AT_SYMLINK_NOFOLLOW ) != AT_SYMLINK_NOFOLLOW )
        fl |= divine::fs::flags::At::Invalid;

    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        divine::fs::filesystem.chmodAt( dirfd, path, mode, fl );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int chmod( const char *path, int mode ) {
    FS_MASK
    return fchmodeat( AT_FDCWD, path, mode, 0 );
}
int fchmod( int fd, int mode ) {
    try {
        divine::fs::filesystem.chmod( fd, mode );
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
        divine::fs::filesystem.closeDirectory( dirp );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int dirfd( DIR *dirp ) {
    FS_MASK
    try {
        return divine::fs::filesystem.getDirectory( dirp )->fd();
    } catch ( Error & ) {
        return -1;
    }
}

DIR *fdopendir( int fd ) {
    FS_MASK
    try {
        return divine::fs::filesystem.openDirectory( fd );
    } catch ( Error & ) {
        return nullptr;
    }
}

DIR *opendir( const char *path ) {
    FS_MASK
    using namespace divine::fs::flags;
    divine::fs::Flags< Open > f = Open::Read;
    try {
        int fd = divine::fs::filesystem.openFileAt( divine::fs::CURRENT_DIRECTORY, path, f );
        return divine::fs::filesystem.openDirectory( fd );
    } catch ( Error & ) {
        return nullptr;
    }
}

struct dirent *readdir( DIR *dirp ) {
    FS_MASK
    static struct dirent entry;

    try {
        auto ent = divine::fs::filesystem.getDirectory( dirp )->get();
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
        auto ent = divine::fs::filesystem.getDirectory( dirp )->get();
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
        divine::fs::filesystem.getDirectory( dirp )->rewind();
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
        int fd = divine::fs::filesystem.openFileAt( divine::fs::CURRENT_DIRECTORY, path, f );
        dirp = divine::fs::filesystem.openDirectory( fd );
        
        struct dirent **entries = nullptr;
        struct dirent *workingEntry = FS_MALLOC( sizeof( struct dirent ) );

        while ( auto ent = divine::fs::filesystem.getDirectory( dirp )->get() ) {
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

        divine::fs::filesystem.closeDirectory( dirp );
        std::qsort( entries, length, sizeof( struct dirent * ), compare );
    } catch ( Error & ) {
        if ( dirp )
            divine::fs::filesystem.closeDirectory( dirp );
        return -1;
    }
}

long telldir( DIR *dirp ) {
    FS_MASK
    e = errno;
    try {
        return divine::fs::filesystem.getDirectory( dirp )->tell();
    } catch ( Error & ) {
        return -1;
    }
}
void seekdir( DIR *dirp, long offset ) {
    FS_MASK
    e = errno;
    try {
        divine::fs::filesystem.getDirectory( dirp )->seek( offset );
    } catch ( Error & ) {
        errno = e;
    }
}

} // extern "C"
