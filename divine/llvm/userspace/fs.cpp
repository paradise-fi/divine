#include <iostream>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>

#include "fs.h"
#include "fcntl.h"
#include "fs-manager.h"

namespace divine {
namespace fs {
extern FSManager filesystem;
} // namespace fs
} // namespace divine

void showFS() {
    using namespace divine::fs;
    int indent = 0;
    std::cout << "FS:" << std::endl;
    filesystem.traverseDirectoryTree( "", 
        [&]( utils::String path ) {
            for ( int i = 0; i < indent; ++i )
                std::cout << "| ";
            utils::String name = utils::splitFileName( path ).second;
            std::cout << '[' << name << ']' << std::endl;
            ++indent;
            return true;
        },
        [&]( utils::String ) { --indent; },
        [&]( utils::String path ) {
            for ( int i = 0; i < indent; ++i )
                std::cout << "| ";
            Node item = filesystem.findDirectoryItem( path, false );
            std::cout << utils::splitFileName( path ).second;
            if ( item->mode().isLink() ) {
                std::cout << " -> " << item->data()->as< Link >()->target();
            }
            else if ( item->mode().isFifo() ) {
                std::cout << " {pipe}";
            }
            std::cout << std::endl;
        }
    );
    std::cout << "-------------------------------------------" << std::endl;
}

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

int creat( const char *path, int mode ) {
    try {
        return divine::fs::filesystem.createFile( path, unsigned( mode ) );
    } catch( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int openat( int dirfd, const char *path, int flags, ... ) {
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
    if ( flags & O_TMPFILE )
        f |= divine::fs::flags::Open::TmpFile;
    if ( dirfd == AT_FDCWD )
        dirfd = divine::fs::CURRENT_DIRECTORY;
    try {
        return divine::fs::filesystem.openFileAt( dirfd, path, f, m );
    } catch ( Error & ) {
        return -1;
    }
}
int open( const char *path, int flags, ... ) {
    int mode = 0;
    if ( flags & O_CREAT ) {
        va_list args;
        va_start( args, flags );
        mode = va_arg( args, int );
        va_end( args );
    }
    return openat( AT_FDCWD, path, flags, mode );
}

int close( int fd ) {
    try {
        divine::fs::filesystem.closeFile( fd );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

ssize_t write( int fd, const void *buf, size_t count ) {
    try {
        auto f = divine::fs::filesystem.getFile( fd );
        return f->write( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}

ssize_t read( int fd, void *buf, size_t count ) {
    try {
        auto f = divine::fs::filesystem.getFile( fd );
        return f->read( buf, count );
    } catch ( Error & ) {
        return -1;
    }
}

int mkdir( const char *path, int mode ) {
    try {
        divine::fs::filesystem.createDirectory( path, mode );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int unlink( const char *path ) {
    try {
        divine::fs::filesystem.removeFile( path );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int rmdir( const char *path ) {
    try {
        divine::fs::filesystem.removeDirectory( path );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int unlinkat( int dirfd, const char *path, int flags ) {
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
    try {
        return divine::fs::filesystem.duplicate( fd );
    } catch ( Error & ) {
        return -1;
    }
}
int dup2( int oldfd, int newfd ) {
    try {
        return divine::fs::filesystem.duplicate2( oldfd, newfd );
    } catch ( Error & ) {
        return -1;
    }
}
int symlink( const char *target, const char *linkpath ) {
    try {
        divine::fs::filesystem.createSymLink( linkpath, target );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}
int link( const char *target, const char *linkpath ) {
    try {
        divine::fs::filesystem.createHardLink( linkpath, target );
        return 0;
    } catch ( Error & ) {
        return -1;
    }
}

int faccessat( int dirfd, const char *path, int mode, int flags ) {
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
    return faccessat( AT_FDCWD, path, mode, 0 );
}

int stat( const char *path, struct stat *buf ) {
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
    try {
        auto item = divine::fs::filesystem.getFile( fd );
        return _fillStat( item->inode(), buf );
    } catch ( Error & ) {
        return -1;
    }
}

unsigned umask( unsigned mask ) {
    unsigned result = divine::fs::filesystem.umask();
    divine::fs::filesystem.umask( mask & 0777 );
    return result;
}

int chdir( const char *path ) {
    try {
        divine::fs::filesystem.changeDirectory( path );
        return 0;
    } catch( Error & ) {
        return -1;
    }
}

int fchdir( int dirfd ) {
    try {
        divine::fs::filesystem.changeDirectory( dirfd );
        return 0;
    } catch( Error & ) {
        return -1;
    }
}

void _exit( int status ) {
    std::_Exit( status );
}

} // extern "C"
