// -*- C++ -*- (c) 2014 Vladimír Štill

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#include <vector>
#include <string>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <memory>

#ifndef BRICK_FS_H
#define BRICK_FS_H

#ifdef _WIN32
#ifndef noexcept
#define noexcept
#endif
#endif

namespace brick {
namespace fs {

enum class AutoDelete : bool { No = false, Yes = true };

struct SystemException : std::exception {
    // make sure we don't override errno accidentaly when constructing std::string
    explicit SystemException( std::string what ) : SystemException{ errno, what } { }
    explicit SystemException( const char *what ) : SystemException{ errno, what } { }

    virtual const char *what() const noexcept override { return _what.c_str(); }

  private:
    std::string _what;

    explicit SystemException( int _errno, std::string what ) {
        _what = "System error: " + std::string( std::strerror( _errno ) ) + ", when " + what;
    }
};

struct Exception : std::runtime_error {
    explicit Exception( std::string what ) : std::runtime_error( what ) { }
    explicit Exception( const char *what ) : std::runtime_error( what ) { }
};

#if defined( __unix )
const char pathSeparators[] = { '/' };
#elif defined( _WIN32 )
const char pathSeparators[] = { '\\', '/' };
#endif

inline bool isPathSeparator( char c ) {
    for ( auto sep : pathSeparators )
        if ( sep == c )
            return true;
    return false;
}

inline std::pair< std::string, std::string > splitExtension( std::string path ) {
    auto pos = path.rfind( '.' );
    if ( pos == std::string::npos )
        return std::make_pair( path, std::string() );
    return std::make_pair( path.substr( 0, pos ), path.substr( pos ) );
}

inline std::string takeExtension( std::string path ) {
    return splitExtension( path ).second;
}

inline std::string dropExtension( std::string path ) {
    return splitExtension( path ).first;
}

inline std::string replaceExtension( std::string path, std::string extension ) {
    if ( !extension.empty() && extension[0] == '.' )
        return dropExtension( path ) + extension;
    return dropExtension( path ) + "." + extension;
}

inline std::pair< std::string, std::string > splitFileName( std::string path ) {
    auto pos = std::find_if( path.rbegin(), path.rend(), &isPathSeparator );
    if ( pos == path.rend() )
        return std::make_pair( std::string(), path );
    auto count = &*pos - &path.front();
    return std::make_pair( path.substr( 0, count ), path.substr( count + 1 ) );
}

inline std::pair< std::string, std::string > absolutePrefix( std::string path ) {
#ifdef _WIN32 /* this must go before general case, because \ is prefix of \\ */
    if ( path.size() >= 3 && path[ 1 ] == ':' && isPathSeparator( path[ 2 ] ) )
        return std::make_pair( path.substr( 0, 3 ), path.substr( 3 ) );
    if ( path.size() >= 2 && isPathSeparator( path[ 0 ] ) && isPathSeparator( path[ 1 ] ) )
        return std::make_pair( path.substr( 0, 2 ), path.substr( 2 ) );
#endif
    // this is absolute path in both windows and unix
    if ( path.size() >= 1 && isPathSeparator( path[ 0 ] ) )
        return std::make_pair( path.substr( 0, 1 ), path.substr( 1 ) );
    return std::make_pair( std::string(), path );
}

inline bool isAbsolute( std::string path ) {
    return absolutePrefix( path ).first.size() != 0;
}

inline bool isRelative( std::string path ) {
    return !isAbsolute( path );
}

inline std::string joinPath( std::vector< std::string > paths ) {
    if ( paths.empty() )
        return "";
    auto it = ++paths.begin();
    std::string out = paths[0];

    for ( ; it != paths.end(); ++it ) {
        if ( isAbsolute( *it ) || out.empty() )
            out = *it;
        else if ( !out.empty() && isPathSeparator( out.back() ) )
            out += *it;
        else
            out += pathSeparators[0] + *it;
    }
    return out;
}

template< typename... FilePaths >
inline std::string joinPath( FilePaths &&...paths ) {
    return joinPath( std::vector< std::string >{ std::forward< FilePaths >( paths )... } );
}

inline std::vector< std::string > splitPath( std::string path ) {
    std::vector< std::string > out;
    auto last = path.begin();
    while ( true ) {
        auto next = std::find_if( last, path.end(), &isPathSeparator );
        if ( next == path.end() ) {
            out.emplace_back( last, next );
            return out;
        }
        if ( last != next )
            out.emplace_back( last, next );
        last = ++next;
    }
}

inline std::string normalize( std::string path ) {
    auto abs = absolutePrefix( path );
    return joinPath( abs.first, joinPath( splitPath( abs.second ) ) );
}


inline std::string getcwd() {
    std::string buf;
#ifdef _WIN32
    char *buffer;
    if ( ( buffer = _getcwd( NULL, 0 ) ) == NULL )
        throw SystemException( "getting the current working directory" );

    buf = buffer;
    free( buffer );
#else
    size_t size = pathconf( ".", _PC_PATH_MAX );
    buf.resize( size );
    if ( ::getcwd( &buf.front(), size ) == nullptr )
        throw SystemException( "getting the current working directory" );
    buf.resize( std::strlen( &buf.front() ) );
#endif
    return buf;
}

inline void chdir( std::string dir ) {
#ifdef _WIN32
    if ( ::_chdir( dir.c_str() ) != 0 )
        throw SystemException( "changing directory" );
#else
    if ( ::chdir( dir.c_str() ) != 0 )
        throw SystemException( "changing directory" );
#endif
}

inline std::string mkdtemp( std::string dirTemplate ) {
#ifdef _WIN32
    if ( ::_mktemp( &dirTemplate.front() ) == nullptr )
        throw SystemException( "creating temporary directory" );
#else
    if ( ::mkdtemp( &dirTemplate.front() ) == nullptr )
        throw SystemException( "creating temporary directory" );
#endif
    return dirTemplate;
}

#ifdef _WIN32
#define stat64 _stat64
#endif
inline std::unique_ptr< struct stat64 > stat( std::string pathname ) {
#if _WIN32
    // from MSDN:
    // If path contains the location of a directory, it cannot contain
    // a trailing backslash. If it does, -1 will be returned and errno
    // will be set to ENOENT.
    pathname = normalize( pathname );
#endif
	std::unique_ptr< struct stat64 > res( new struct stat64 );
	if ( ::stat64( pathname.c_str(), res.get() ) == -1 ) {
		if ( errno == ENOENT )
			return std::unique_ptr< struct stat64 >();
		else
			throw SystemException( "getting file information for " + pathname );
    }
	return res;
}

#ifdef _WIN32
inline void mkdirIfMissing( std::string dir, int ) {
#else
inline void mkdirIfMissing( std::string dir, mode_t mode ) {
#endif
    for ( int i = 0; i < 5; ++i )
    {
        // If it does not exist, make it
#ifdef _WIN32
        if ( ::_mkdir( dir.c_str() ) != -1 )
#else
        if ( ::mkdir( dir.c_str(), mode ) != -1 )
#endif
            return;

        // throw on all errors except EEXIST. Note that EEXIST "includes the case
        // where pathname is a symbolic link, dangling or not."
        if ( errno != EEXIST )
            throw SystemException( "creating directory " + dir );

        // Ensure that, if dir exists, it is a directory
        auto st = stat( dir );

        if ( !st ) {
            // Either dir has just been deleted, or we hit a dangling
            // symlink.
            //
            // Retry creating a directory: the more we keep failing, the more
            // the likelyhood of a dangling symlink increases.
            //
            // We could lstat here, but it would add yet another case for a
            // race condition if the broken symlink gets deleted between the
            // stat and the lstat.
            continue;
        }
#ifdef _WIN32
        else if ( ( st->st_mode & _S_IFDIR ) == 0 )
#else
        else if ( !S_ISDIR( st->st_mode ) )
#endif
            // If it exists but it is not a directory, complain
            throw Exception( "ensuring path: " + dir + " exists but it is not a directory" );
        else
            // If it exists and it is a directory, we're fine
            return;
    }
    throw Exception( "ensuring path: " + dir + " exists and looks like a dangling symlink" );
}

inline void mkpath( std::string dir ) {
    std::pair< std::string, std::string > abs = absolutePrefix( dir );
    auto split = splitPath( abs.second );
    std::vector< std::string > toDo;
    for ( auto &x : split ) {
        toDo.emplace_back( x );
        mkdirIfMissing( abs.first + joinPath( toDo ), 0777 );
    }
}

inline void mkFilePath( std::string file ) {
    auto dir = splitFileName( file ).first;
    if ( !dir.empty() )
        mkpath( dir );
}

#if 0
struct Fd {
    template< typename... Args >
    Fd( Args... args ) : _fd( ::open( args... ) ) {
        if ( _fd < 0 )
            throw SystemException( "while opening file" );
    }
    ~Fd() { close(); }

    void close() {
        if ( _fd >= 0 ) {
            ::close( _fd );
            _fd = -1;
        }
    }

    operator int() const { return _fd; }
    int fd() const { return _fd; }

  private:
    int _fd;
};
#endif

inline void writeFile( std::string file, std::string data ) {
    std::ofstream out( file.c_str(), std::ios::binary );
    if ( !out.is_open() )
        throw SystemException( "writing file " + file );
    out << data;
    if ( !out.good() )
        throw SystemException( "writing data to file " + file );
}

inline void renameIfExists( std::string src, std::string dst ) {
    int res = ::rename( src.c_str(), dst.c_str() );
    if ( res < 0 && errno != ENOENT )
        throw SystemException( "moving " + src + " to " + dst );
}

inline void unlink( std::string fname )
{
#ifdef _WIN32
    if ( ::_unlink( fname.c_str() ) < 0 )
        throw SystemException( "cannot delete file" + fname );
#else
    if ( ::unlink( fname.c_str() ) < 0 )
        throw SystemException( "cannot delete file" + fname );
#endif
}

inline void rmdir( std::string dirname ) {
#ifdef _WIN32
    if ( ::_rmdir( dirname.c_str() ) < 0 )
        throw SystemException( "cannot delete directory " + dirname );
#else
    if ( ::rmdir( dirname.c_str() ) < 0 )
        throw SystemException( "cannot delete directory " + dirname );
#endif
}

#ifdef _WIN32
} // fs
} // brick
/*

Declaration of POSIX directory browsing functions and types for Win32.

Author:  Kevlin Henney (kevlin@acm.org, kevlin@curbralan.com)
History: Created March 1997. Updated June 2003.

Copyright Kevlin Henney, 1997, 2003. All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose is hereby granted without fee, provided
that this copyright and permissions notice appear in all copies and
derivatives.

This software is supplied "as is" without express or implied warranty.

But that said, if there are any problems please get in touch.
*/
#ifdef __cplusplus
extern "C"
{
#endif
struct dirent {
    char *d_name;
};
struct DIR {
    intptr_t            handle; /* -1 for failed rewind */
    struct _finddata_t  info;
    struct dirent       result; /* d_name null iff first time */
    char                *name;  /* null-terminated char string */
};

DIR *opendir( const char *name ) {
    DIR *dir = 0;

    if ( name && name[ 0 ] ) {
        size_t base_length = strlen( name );
        const char *all = /* search pattern must end with suitable wildcard */
            strchr( "/\\", name[ base_length - 1 ] ) ? "*" : "/*";

        if ( ( dir = (DIR *)malloc( sizeof *dir ) ) != 0 &&
             ( dir->name = (char *)malloc( base_length + strlen( all ) + 1 ) ) != 0 ) {
            strcat( strcpy( dir->name, name ), all );

            if ( ( dir->handle =
                (intptr_t)_findfirst( dir->name, &dir->info ) ) != -1 ) {
                dir->result.d_name = 0;
            }
            else { /* rollback */
                free( dir->name );
                free( dir );
                dir = 0;
            }
        }
        else { /* rollback */
            free( dir );
            dir = 0;
            errno = ENOMEM;
        }
    }
    else {
        errno = EINVAL;
    }

    return dir;
}

int closedir( DIR *dir ) {
    int result = -1;

    if ( dir ) {
        if ( dir->handle != -1 ) {
            result = _findclose( dir->handle );
        }

        free( dir->name );
        free( dir );
    }

    if ( result == -1 ) {/* map all errors to EBADF */
        errno = EBADF;
    }
    return result;
}

struct dirent *readdir( DIR *dir ) {
    struct dirent *result = 0;

    if ( dir && dir->handle != -1 ) {
        if ( !dir->result.d_name || _findnext( dir->handle, &dir->info ) != -1 ) {
           result = &dir->result;
           result->d_name = dir->info.name;
        }
    }
    else {
        errno = EBADF;
    }
    return result;
}

void rewinddir( DIR *dir ) {
    if ( dir && dir->handle != -1 ) {
        _findclose( dir->handle );
        dir->handle = (intptr_t)_findfirst( dir->name, &dir->info );
        dir->result.d_name = 0;
    }
    else {
        errno = EBADF;
    }
}

#ifdef __cplusplus
}
#endif

namespace brick {
namespace fs {
#endif

template< typename DirPre, typename DirPost, typename File >
void traverseDirectoryTree( std::string root, DirPre pre, DirPost post, File file ) {
    if ( pre( root ) ) {
        auto dir = std::unique_ptr< DIR, decltype( &::closedir ) >(
                            ::opendir( root.c_str() ), &::closedir );
        if ( dir == nullptr )
            throw SystemException( "opening directory " + root );

        for ( auto de = readdir( dir.get() ); de != nullptr; de = readdir( dir.get() ) ) {
            std::string name = de->d_name;
            if ( name == "." || name == ".." )
                continue;

            auto path = joinPath( root, name );
            auto st = stat( path );
#ifdef _WIN32
            if ( st->st_mode & _S_IFDIR )
#else
            if ( S_ISDIR( st->st_mode ) )
#endif
                traverseDirectoryTree( path, pre, post, file );
            else
                file( path );
        }

        post( root );
    }
}

template< typename Dir, typename File >
void traverseDirectory( std::string root, Dir dir, File file ) {
    traverseDirectoryTree( root, [&]( std::string d ) -> bool {
            if ( d == root )
                return true;
            else
                dir( d );
            return false;
        }, []( std::string ) {}, file );
}

template< typename File >
void traverseFiles( std::string dir, File file ) {
    traverseDirectory( dir, []( std::string ) {}, file );
}


inline void rmtree( std::string dir ) {
    traverseDirectoryTree( dir, []( std::string ) { return true; },
            []( std::string dir ) { rmdir( dir ); },
            []( std::string file ) { unlink( file ); } );
}

struct ChangeCwd {
    ChangeCwd( std::string newcwd ) : oldcwd( getcwd() ) {
        chdir( newcwd );
    }
    ~ChangeCwd() {
        chdir( oldcwd );
    }

    const std::string oldcwd;
};

struct TempDir {
    TempDir( std::string nameTemplate, AutoDelete autoDelete ) :
        path( mkdtemp( nameTemplate ) ), autoDelete( autoDelete )
    { }
    ~TempDir() {
        if ( bool( autoDelete ) )
            rmtree( path );
    }

    const std::string path;
    AutoDelete autoDelete;
};

namespace {

std::string readFile( std::ifstream &in ) {
    if ( !in.is_open() )
        throw std::runtime_error( "reading filestream" );
    size_t length;

    in.seekg( 0, std::ios::end );
    length = in.tellg();
    in.seekg( 0, std::ios::beg );

    std::string buffer;
    buffer.resize( length );

    in.read( &buffer[ 0 ], length );
    return buffer;
}

std::string readFile( const std::string &file ) {
    std::ifstream in( file.c_str(), std::ios::binary );
    if ( !in.is_open() )
        throw std::runtime_error( "reading file " + file );
    return readFile( in );
}

}

#ifdef _WIN32
#define F_OK 0
#define R_OK 2
#define W_OK 4
//#define X_OK
#endif


inline bool access(const std::string &s, int m)
{
#ifdef _WIN32
    return ::_access(s.c_str(), m) == 0;
#else
    return ::access(s.c_str(), m) == 0;
#endif
}

inline bool exists(const std::string& file)
{
    return access(file, F_OK);
}

}
}

#if 0
inline std::pair< std::string, std::string > splitExtension( std::string path ) {
inline std::string replaceExtension( std::string path, std::string extension ) {
inline std::pair< std::string, std::string > splitFileName( std::string path ) {
inline std::pair< std::string, std::string > absolutePrefix( std::string path ) {
#endif

#endif // BRICK_FS_H
