#include <iostream>
#include <cerrno>
#include <cstdarg>

#include "fs.h"
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
            Ptr item = filesystem.findDirectoryItem( path, false );
            std::cout << item->name();
            if ( item->isSymLink() ) {
                std::cout << " -> " << item->as< SymLink >()->target();
            }
            else if ( item->isPipe() ) {
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
    buf->st_uid = 0;
    buf->st_gid = 0;
    buf->st_rdev = 0;
    buf->st_atime = 0;
    buf->st_mtime = 0;
    buf->st_ctime = 0;
}

static int _fillStat( divine::fs::ConstPtr item, struct stat *buf ) {
    if ( !item )
        return -1;
    _initStat( buf );
    buf->st_ino = item->ino();
    buf->st_mode = item->grants();
    buf->st_nlink = item->isFile() ? item->as< divine::fs::FileItem >()->object().use_count() : 1;
    buf->st_size = item->size();
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

int open( const char *path, int flags, ... ) {
    divine::fs::Flags< divine::fs::flags::Open > f = divine::fs::flags::Open::NoFlags;
    unsigned m = 0;
    va_list args;
    va_start( args, flags );
    if ( flags & O_RDONLY )  f |= divine::fs::flags::Open::Read;
    if ( flags & O_WRONLY )  f |= divine::fs::flags::Open::Write;
    if ( flags & O_CREAT ) { f |= divine::fs::flags::Open::Create; m = va_arg( args, unsigned ); }
    if ( flags & O_EXCL )    f |= divine::fs::flags::Open::Excl;
    if ( flags & O_TMPFILE ) f |= divine::fs::flags::Open::TmpFile;
    va_end( args );

    try {
        return divine::fs::filesystem.openFile( path, f, m );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int close( int fd ) {
    try {
        divine::fs::filesystem.closeFile( fd );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

ssize_t write( int fd, const void *buf, size_t count ) {
    try {
        auto f = divine::fs::filesystem.getFile( fd );
        return f->write( buf, count );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

ssize_t read( int fd, void *buf, size_t count ) {
    try {
        auto f = divine::fs::filesystem.getFile( fd );
        return f->read( buf, count );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int mkdir( const char *path, int mode ) {
    try {
        divine::fs::filesystem.createDirectory( path, mode );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int unlink( const char *path ) {
    try {
        divine::fs::filesystem.removeFile( path );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int rmdir( const char *path ) {
    try {
        divine::fs::filesystem.removeDirectory( path );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
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
    } catch ( Error &e ) {
        errno = e.code();
        return off_t( -1 );
    }
}
int dup( int fd ) {
    try {
        return divine::fs::filesystem.duplicate( fd );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}
int dup2( int oldfd, int newfd ) {
    try {
        return divine::fs::filesystem.duplicate2( oldfd, newfd );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}
int symlink( const char *target, const char *linkpath ) {
    try {
        divine::fs::filesystem.createSymLink( linkpath, target );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}
int link( const char *target, const char *linkpath ) {
    try {
        divine::fs::filesystem.createHardLink( linkpath, target );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}
int access( const char *path, int mode ) {
    divine::fs::Flags< divine::fs::flags::Access > m = divine::fs::flags::Access::OK;
    if ( mode & R_OK )  m |= divine::fs::flags::Access::Read;
    if ( mode & W_OK )  m |= divine::fs::flags::Access::Write;
    if ( mode & X_OK )  m |= divine::fs::flags::Access::Execute;
    try {
        divine::fs::filesystem.access( path, m );
        return 0;
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int stat( const char *path, struct stat *buf ) {
    try {
        auto item = divine::fs::filesystem.findDirectoryItem( path );
        if ( !item )
            throw Error( ENOENT );
        return _fillStat( item, buf );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int lstat( const char *path, struct stat *buf ) {
    try {
        auto item = divine::fs::filesystem.findDirectoryItem( path, false );
        if ( !item )
            throw Error( ENOENT );
        return _fillStat( item, buf );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}

int fstat( int fd, struct stat *buf ) {
    try {
        auto item = divine::fs::filesystem.getFile( fd );
        return _fillStat( item->directoryItem(), buf );
    } catch ( Error &e ) {
        errno = e.code();
        return -1;
    }
}


} // extern "C"
