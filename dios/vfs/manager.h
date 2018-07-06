// -*- C++ -*- (c) 2015 Jiří Weiser

#include <tuple>
#include <memory>
#include <algorithm>
#include <utility>
#include <array>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dios/sys/main.hpp>
#include <bits/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dios.h>
#include <sys/vmutil.h>
#include <string_view>

#include "utils.h"
#include "inode.h"
#include "constants.h"
#include "file.h"
#include "directory.h"
#include "snapshot.h"
#include "descriptor.h"
#include "path.h"
#include "syscall.hpp"

#ifndef _FS_MANAGER_H_
#define _FS_MANAGER_H_

namespace __dios {
namespace fs {

struct Manager {

    Manager() : Manager( true ) {}

    void setOutputFile(FileTrace trace);
    void setErrFile(FileTrace trace);

    Node findDirectoryItem( __dios::String name, bool followSymLinks = true );
    void initializeFromSnapshot(const _VM_Env *env);

    void createHardLinkAt( int newdirfd, __dios::String name, int olddirfd, const __dios::String &target, bool follow );
    void createSymLinkAt( int dirfd, __dios::String name, __dios::String target );
    template< typename... Args >
    Node createNodeAt( int dirfd, __dios::String name, Mode mode, Args &&... args );

    ssize_t readLinkAt( int dirfd, __dios::String name, char *buf, size_t count );

    void accessAt( int dirfd, __dios::String name, int mode, bool follow );
    void closeFile( int fd );
    int duplicate( int oldfd, int lowEdge = 0 );
    int duplicate2( int oldfd, int newfd );
    std::shared_ptr< FileDescriptor > getFile( int fd );
    std::shared_ptr< Socket > getSocket( int sockfd );

    std::pair< int, int > pipe();

    void removeFile( int dirfd, __dios::String name );
    void removeDirectory( int dirfd, __dios::String name );

    void renameAt( int newdirfd, __dios::String newpath, int olddirfd, __dios::String oldpath );

    off_t lseek( int fd, off_t offset, Seek whence );

    Node currentDirectory() {
        return _currentDirectory.lock();
    }

    void getCurrentWorkingDir( char *buff, size_t size );

    void changeDirectory( __dios::String pathname );
    void changeDirectory( int dirfd );

    Mode umask() const {
        return _proc->_umask;
    }
    void umask( Mode mask ) {
        _proc->_umask = ACCESSPERMS & mask;
    }

    int socket( SocketType type, OFlags fl );
    std::pair< int, int > socketpair( SocketType type, OFlags fl );
    void bind( int sockfd, Socket::Address address );
    void connect( int sockfd, const Socket::Address &address );
    int accept( int sockfd, Socket::Address &address );
    Node resolveAddress( const Socket::Address &address );
    bool wasError() const {
        return _error == 0;
    }

    int getError() const {
        return _error;
    }

    void setError(int code) {
        _error = code;
    }

    template< typename U > friend struct VFS;
    Node _root;
    int _error;
    WeakNode _currentDirectory;
    std::array< Node, 3 > _standardIO;

    ProcessInfo *_proc;

    Manager( bool );// private default ctor

    std::pair< Node, __dios::String > _findDirectoryOfFile( __dios::String name );

    template< typename I >
    Node _findDirectoryItem( __dios::String name, bool followSymLinks, I itemChecker );

    int _getFileDescriptor( Node node, OFlags flags, int lowEdge = 0 );
    int _getFileDescriptor( std::shared_ptr< FileDescriptor >, int lowEdge = 0 );
    void _insertSnapshotItem( const SnapshotFS &item );

    void _checkGrants( Node inode, Mode grant ) const;
};

namespace conversion {

    using namespace __dios::fs::flags;

    static inline LegacyFlags< Message > message( int fls )
    {
        LegacyFlags< Message > f = Message::NoFlags;

        if ( fls & MSG_DONTWAIT ) f |= Message::DontWait;
        if ( fls & MSG_PEEK ) f |= Message::Peek;
        if ( fls & MSG_WAITALL ) f |= Message::WaitAll;
        return f;
    }

    static_assert( AT_FDCWD == __dios::fs::CURRENT_DIRECTORY,
                   "mismatch value of AT_FDCWD and __dios::fs::CURRENT_DIRECTORY" );
}

template < typename Next >
struct VFS: Syscall, Next
{
    VFS() { _manager = new( __dios::nofail ) Manager{}; }
    VFS( const VFS& ) = delete;
    ~VFS() {
        if ( _manager )
            delete _manager;
    }
    VFS& operator=( const VFS& ) = delete;

    Manager &instance() {
        __FS_assert( _manager );

        if ( this->getCurrentTask() )
            _manager->_proc = static_cast< Process* >( this->getCurrentTask()->_proc );
        return *_manager;
    }

    struct Process : Next::Process, fs::ProcessInfo
    {};

    Node root() override { return _root; }
    Node cwd() override { return instance().currentDirectory(); }
    ProcessInfo &proc() override { return *static_cast< Process * >( this->getCurrentTask()->_proc ); }

    using Syscall::open;
    using Syscall::openat;

    using Syscall::chmod;
    using Syscall::fchmod;
    using Syscall::fchmodat;

    using Syscall::read;
    using Syscall::write;

    using Syscall::truncate;
    using Syscall::ftruncate;

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< VFS >( "{VFS}" );

        auto &sio = instance()._standardIO;

        for ( auto env = s.env ; env->key; env++ )
            if ( !strcmp( env->key, "vfs.stdin" ) )
                sio[ 0 ].reset( new( __dios::nofail ) StandardInput( env->value, env->size ) );

        instance().setOutputFile( getFileTraceConfig( s.opts, "stdout" ) );
        instance().setErrFile( getFileTraceConfig( s.opts, "stderr" ) );
        instance().initializeFromSnapshot( s.env );

        for ( int i = 0; i < 2; ++i )
            sio[ i ]->mode( S_IFREG | S_IRUSR );

        s.proc1->_openFD = __dios::Vector< std::shared_ptr< FileDescriptor > > ( {
        std::allocate_shared< FileDescriptor >( __dios::AllocatorPure(), sio[ 0 ], O_RDONLY ),
        std::allocate_shared< FileDescriptor >( __dios::AllocatorPure(), sio[ 1 ], O_WRONLY ),
        std::allocate_shared< FileDescriptor >( __dios::AllocatorPure(), sio[ 2 ], O_WRONLY )
        } );
        s.proc1->_umask = S_IWGRP | S_IWOTH;

        _root = instance()._root;
        Next::setup( s );
    }

     void getHelp( Map< String, HelpOption >& options ) {
        const char *opt1 = "{stdout|stderr}";
        if ( options.find( opt1 ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt1 );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        };

        options[ { opt1 } ] = { "specify how to treat program output",
          { "notrace - ignore the stream",
            "unbuffered - trace each write",
            "trace - trace after each newline (default)"} };

        const char *opt2 = "syscall";
        if ( options.find( opt2 ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt2 );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        };

        options[ { opt2 } ] = { "specify how to treat standard syscalls",
          { "simulate - simulate syscalls, use virtual file system (can be used with verify and run)",
            "passthrough - use syscalls from the underlying host OS (cannot be used with verify) " } };

        Next::getHelp( options );
    }

    void finalize()
    {
        delete _manager;
        _manager = nullptr;
        Next::finalize();
    }

    static FileTrace getFileTraceConfig( SysOpts& o, String stream ) {
        auto r = std::find_if( o.begin(), o.end(), [&]( const auto& o ) {
            return o.first == stream;
        } );
        if ( r == o.end() )
            return FileTrace::TRACE;
        String s = r->second;
        o.erase( r );
        if ( s == "notrace" )
            return FileTrace::NOTRACE;
        if ( s == "unbuffered" )
            return FileTrace::UNBUFFERED;
        if ( s == "trace" )
            return FileTrace::TRACE;
        __dios_trace_f( "Invalid configuration for file %s", stream.c_str() );
        __dios_fault( _DiOS_F_Config, "Invalid file tracing configuration" );
        __builtin_unreachable();
    }

    static void _initStat( struct stat *buf )
    {
        buf->st_dev = 0;
        buf->st_rdev = 0;
        buf->st_atime = 0;
        buf->st_mtime = 0;
        buf->st_ctime = 0;
    }

    static int _fillStat( const __dios::fs::Node item, struct stat *buf )
    {
        if ( !item )
            return -1;
        _initStat( buf );
        buf->st_ino = item->ino( );
        buf->st_mode = item->mode().to_i();
        buf->st_nlink = item.use_count( );
        buf->st_size = item->size( );
        buf->st_uid = item->uid( );
        buf->st_gid = item->gid( );
        buf->st_blksize = 512;
        buf->st_blocks = ( buf->st_size + 1 ) / buf->st_blksize;
        return 0;
    }

private: /* helper methods */

    Node _root;

public: /* system call implementation */

    int creat( const char *path, Mode mode )
    {
        return mknodat( AT_FDCWD, path, mode | S_IFREG, 0 );
    }

    int fcntl( int fd, int cmd, va_list *vl )
    {
        try {
            auto f = instance( ).getFile( fd );

            switch ( cmd ) {
                case F_SETFD:
                    va_end( *vl );
                case F_GETFD:
                    return 0;
                case F_DUPFD_CLOEXEC: // for now let assume we cannot handle O_CLOEXEC
                    return -1;
                case F_DUPFD:
                {
                    int lowEdge = va_arg(  *vl, int );
                    va_end( *vl );
                    return instance( ).duplicate( fd, lowEdge );
                }
                case F_GETFL:
                    va_end( *vl );
                    return f->flags().to_i();
                case F_SETFL:
                {
                    OFlags mode = va_arg( *vl, int );

                    if ( mode & ~( O_APPEND | O_NONBLOCK ) )
                        return error( EINVAL ), -1;
                    if ( !( mode & O_APPEND ) && ( f->flags() & O_APPEND ) )
                        return error( EPERM ), -1;

                    f->flags() &= ~( O_APPEND | O_NONBLOCK );
                    f->flags() |= mode;

                    va_end( *vl );
                    return 0;
                }
                default:
                    __dios_trace_f( "the fcntl command %d is not implemented", cmd );
                    va_end( *vl );
                    return -1;
            }

        } catch ( Error & e ) {
            va_end( *vl );
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int close( int fd )
    {
        if ( check_fd( fd, F_OK ) )
            return instance().closeFile( fd ), 0;
        else
            return -1;
    }

    int pipe( int *pipefd )
    {
        try {
            std::tie( pipefd[ 0 ], pipefd[ 1 ] ) = instance( ).pipe( );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    off_t lseek( int fd, off_t offset, int whence )
    {
        if ( !check_fd( fd, F_OK ) )
            return -1;
        try {
            __dios::fs::Seek w = __dios::fs::Seek::Undefined;
            switch ( whence ) {
                case SEEK_SET:
                    w = __dios::fs::Seek::Set;
                    break;
                case SEEK_CUR:
                    w = __dios::fs::Seek::Current;
                    break;
                case SEEK_END:
                    w = __dios::fs::Seek::End;
                    break;
            }
            return instance( ).lseek( fd, offset, w );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int dup( int fd )
    {
       try {
            return instance( ).duplicate( fd );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int dup2( int oldfd, int newfd )
    {
        try {
            return instance( ).duplicate2( oldfd, newfd );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int unlink( const char *path )
    {
        return unlinkat( AT_FDCWD, path, 0 );
    }

    int rmdir( const char *path )
    {
        return unlinkat( AT_FDCWD, path, AT_REMOVEDIR );
    }

    int unlinkat( int dirfd, const char *path, int flags )
    {
        try {
            if ( flags == 0 )
                instance().removeFile( dirfd, path );
            else if ( flags == AT_REMOVEDIR )
                instance().removeDirectory( dirfd, path );
            else
                return error_negative( EINVAL );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int linkat( int olddirfd, const char *target, int newdirfd, const char * linkpath, int flags )
    {
        if ( ( flags | AT_SYMLINK_FOLLOW ) != AT_SYMLINK_FOLLOW )
            return error_negative( EINVAL );
        try {
            instance().createHardLinkAt( newdirfd, linkpath, olddirfd, target,
                                         flags & AT_SYMLINK_FOLLOW );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int link( const char *target, const char *linkpath )
    {
        return linkat( AT_FDCWD, target, AT_FDCWD, linkpath, 0 );
    }

    int symlinkat( const char *target, int dirfd, const char *linkpath )
    {
        try {
            instance( ).createSymLinkAt( dirfd, linkpath, target );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int symlink( const char *target, const char *linkpath )
    {
        return symlinkat( target, AT_FDCWD, linkpath );
    }

    ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t count )
    {
        try {
            return instance( ).readLinkAt( dirfd, path, buf, count );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    ssize_t readlink( const char *path, char *buf, size_t count )
    {
        return readlinkat( AT_FDCWD, path, buf, count );
    }

    bool check_access( int mode )
    {
        return ( mode | R_OK | W_OK | X_OK ) == ( R_OK | W_OK | X_OK );
    }

    int faccessat( int dirfd, const char *path, int mode, int flags )
    {
        if ( !check_access( mode ) )
            return error_negative( EINVAL );

        if ( ( flags | AT_EACCESS | AT_SYMLINK_NOFOLLOW ) != ( AT_EACCESS | AT_SYMLINK_NOFOLLOW ) )
            return error_negative( EINVAL );

        try {
            /* FIXME handle AT_EACCESS */
            instance( ).accessAt( dirfd, path, mode, !( flags & AT_SYMLINK_NOFOLLOW ) );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int access( const char *path, int mode )
    {
        return faccessat( AT_FDCWD, path, mode, 0 );
    }

    int chdir( const char *path )
    {
        try {
            instance( ).changeDirectory( path );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int fchdir( int dirfd )
    {
        try {
            instance( ).changeDirectory( dirfd );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int fdatasync( int fd  )
    {
        return fsync( fd ); /* same thing for us */
    }

    int fsync( int fd )
    {
        if ( !check_fd( fd, F_OK ) )
            return -1;
        else
            return 0;
    }

    int syncfs( int fd )
    {
        if ( !check_fd( fd, F_OK ) )
            return -1;
        else
            return 0;
    }

    void sync()
    {
    }

    int stat( const char *path, struct stat *buf )
    {
        try {
            auto item = instance( ).findDirectoryItem( path );
            if ( !item )
                throw Error( ENOENT );
            return _fillStat( item, buf );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int lstat( const char *path, struct stat *buf  )
    {
        try {
            auto item = instance( ).findDirectoryItem( path, false );
            if ( !item )
                throw Error( ENOENT );
            return _fillStat( item, buf );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int fstat( int fd_, struct stat *buf )
    {
        if ( auto fd = check_fd( fd_, F_OK ) )
            return _fillStat( fd->inode(), buf );
        else
            return -1;
    }

    int fstatfs( int, struct statfs* )
    {
        __dios_trace_t( "fstatfs() is not implemented" );
        return -1;
    }

    int statfs( const char *, struct statfs * )
    {
        __dios_trace_t( "statfs() is not implemented" );
        return -1;
    }

    int mkdirat( int dirfd, const char *path, Mode mode )
    {
        try {
            instance( ).createNodeAt( dirfd, path, ( ACCESSPERMS & mode ) | S_IFDIR );
            return  0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return  -1;
        }
    }

    int mkdir( const char *path, Mode mode )
    {
        return mkdirat( AT_FDCWD, path, mode );
    }

    int mknodat( int dirfd, const char *path, Mode mode, dev_t dev )
    {
        if ( dev != 0 )
            return error( EINVAL ), -1;
        if ( !mode.is_char() && !mode.is_block() && !mode.is_file() && !mode.is_fifo() &&
             !mode.is_socket() )
            return error( EINVAL ), -1;

        try {
            instance().createNodeAt( dirfd, path, mode );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return  -1;
        }
    }

    int mknod( const char *path, Mode mode, dev_t dev )
    {
        return mknodat( AT_FDCWD, path, mode, dev );
    }

    int umask( Mode mask )
    {
        Mode result = instance().umask();
        instance( ).umask( mask & ACCESSPERMS );
        return result.to_i();
    }

    int socket( int domain, int t, int protocol )
    {
        using SocketType = __dios::fs::SocketType;
        using namespace __dios::fs::flags;
        try {
            if ( domain != AF_UNIX )
                throw Error( EAFNOSUPPORT );

            SocketType type;
            switch ( t & __SOCK_TYPE_MASK ) {
                case SOCK_STREAM:
                    type = SocketType::Stream;
                    break;
                case SOCK_DGRAM:
                    type = SocketType::Datagram;
                    break;
                default:
                    throw Error( EPROTONOSUPPORT );
            }
            if ( protocol )
                throw Error( EPROTONOSUPPORT );

            return instance().socket( type, t & SOCK_NONBLOCK ? O_NONBLOCK : 0 );

        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int socketpair( int domain, int t, int protocol, int *fds )
    {
        using namespace conversion;
        try {

            if ( domain != AF_UNIX )
                throw Error( EAFNOSUPPORT );

            SocketType type;
            switch ( t & __SOCK_TYPE_MASK ) {
                case SOCK_STREAM:
                    type = SocketType::Stream;
                    break;
                case SOCK_DGRAM:
                    type = SocketType::Datagram;
                    break;
                default:
                    throw Error( EPROTONOSUPPORT );
            }
            if ( protocol )
                throw Error( EPROTONOSUPPORT );

            std::tie( fds[ 0 ], fds[ 1 ] ) =
                instance().socketpair( type, t & SOCK_NONBLOCK ? O_NONBLOCK : 0 );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int getsockname( int sockfd, struct sockaddr *addr, socklen_t *len )
    {
        try {
            if ( !len )
                throw Error( EFAULT );

            auto s = instance( ).getSocket( sockfd );
            struct sockaddr_un *target = reinterpret_cast< struct sockaddr_un * >( addr );

            auto &address = s->address( );

            if ( address.size( ) >= *len )
                throw Error( ENOBUFS );

            if ( target ) {
                target->sun_family = AF_UNIX;
                char *end = std::copy( address.value( ).begin( ), address.value( ).end( ), target->sun_path );
                *end = '\0';
            }
            *len = address.size( ) + 1 + sizeof( target->sun_family );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int bind( int sockfd, const struct sockaddr * addr, socklen_t )
    {
        using Address = __dios::fs::Socket::Address;

        try {

            if ( !addr )
                throw Error( EFAULT );
            if ( addr->sa_family != AF_UNIX )
                throw Error( EINVAL );

            const struct sockaddr_un *target = reinterpret_cast< const struct sockaddr_un * >( addr );
            Address address( target->sun_path );

            instance( ).bind( sockfd, std::move( address ));
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int connect( int sockfd, const struct sockaddr *addr, socklen_t )
    {
        using Address = __dios::fs::Socket::Address;

        try {

            if ( !addr )
                throw Error( EFAULT );
            if ( addr->sa_family != AF_UNIX )
                throw Error( EAFNOSUPPORT );

            const struct sockaddr_un *target = reinterpret_cast< const struct sockaddr_un * >( addr );
            Address address( target->sun_path );

            instance( ).connect( sockfd, address );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int getpeername( int sockfd, struct sockaddr *addr, socklen_t *len )
    {
        try {
            if ( !len )
                throw Error( EFAULT );

            auto s = instance( ).getSocket( sockfd );
            struct sockaddr_un *target = reinterpret_cast< struct sockaddr_un * >( addr );

            if ( !s->peer() )
                return error( ENOTCONN ), -1;
            auto &address = s->peer()->address( );

            if ( address.size( ) >= *len )
                throw Error( ENOBUFS );

            if ( target ) {
                target->sun_family = AF_UNIX;
                char *end = std::copy( address.value( ).begin( ), address.value( ).end( ), target->sun_path );
                *end = '\0';
            }
            *len = address.size( ) + 1 + sizeof( target->sun_family );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    size_t _send( FileDescriptor &fd, Socket &socket, const char *buffer, size_t length,
                  LegacyFlags< flags::Message > fls )
    {
        if ( fd.flags().nonblock() && !socket.canWrite() )
            throw Error( EAGAIN );

        if ( fd.flags().nonblock() )
            fls |= flags::Message::DontWait;

        socket.send( buffer, length, fls );
        return length;
    }

    size_t _sendto( FileDescriptor &fd, Socket &socket, const char *buffer, size_t length,
                    LegacyFlags< flags::Message > fls, Node node )
    {
        if ( fd.flags().nonblock() && !socket.canWrite() )
            throw Error( EAGAIN );

        if ( fd.flags().nonblock() )
            fls |= flags::Message::DontWait;

        socket.sendTo( buffer, length, fls, node );
        return length;
    }

    ssize_t sendto( int sockfd, const void *buf, size_t n, int flags, const struct sockaddr * addr,
                    socklen_t )
    {
        using Address = __dios::fs::Socket::Address;

        if ( !addr ) {
            try {
                auto s = instance( ).getSocket( sockfd );
                return _send( *instance().getFile( sockfd ), *s, static_cast< const char * >( buf ),
                              n, conversion::message( flags ));
            } catch ( Error & e ) {
                *__dios_errno() = e.code();
                return -1;
            }
        }

        try {
            if ( addr->sa_family != AF_UNIX )
                throw Error( EAFNOSUPPORT );

            auto s = instance( ).getSocket( sockfd );
            const struct sockaddr_un *target = reinterpret_cast< const struct sockaddr_un * >( addr );
            Address address( target ? target->sun_path : __dios::String( ));

            return _sendto( *instance().getFile( sockfd ), *s, static_cast< const char * >( buf ), n,
                            conversion::message( flags ), instance( ).resolveAddress( address ) );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    size_t _receive( FileDescriptor &fd, Socket &socket, char *buffer, size_t length,
                     LegacyFlags< flags::Message > fls, Socket::Address &address )
    {
        if ( fd.flags().nonblock() && !socket.canRead() )
            throw Error( EAGAIN );

        if ( fd.flags().nonblock() )
            fls |= flags::Message::DontWait;

        socket.receive( buffer, length, fls, address );
        return length;
    }

    ssize_t _recvfrom(int sockfd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *len )
    {
        using Address = __dios::fs::Socket::Address;
        Address address;
        struct sockaddr_un *target = reinterpret_cast< struct sockaddr_un * >( addr );
        if ( target && !len )
            throw Error( EFAULT );

        auto s = instance( ).getSocket( sockfd );
        n = _receive( *instance().getFile( sockfd ), *s, static_cast< char * >( buf ), n,
                      conversion::message( flags ), address );

        if ( target ) {
            target->sun_family = AF_UNIX;
            char *end = std::copy( address.value( ).begin( ), address.value( ).end( ), target->sun_path );
            *end = '\0';
            *len = address.size( ) + 1 + sizeof( target->sun_family );
        }
        return n;
    }

    ssize_t recv( int sockfd, void *buf, size_t n, int flags )
    {
        try {
             return _recvfrom( sockfd, buf, n, flags, nullptr, nullptr );
        }catch( Error & e ){
             *__dios_errno() = e.code();
            return -1;
        }
    }

    ssize_t recvfrom( int sockfd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *len )
    {
        try {
             return _recvfrom( sockfd, buf, n, flags, addr, len );
        }catch( Error & e ){
             *__dios_errno() = e.code();
            return -1;
        }
    }

    int listen( int sockfd, int n )
    {
        try {
            auto s = instance( ).getSocket( sockfd );
            s->listen( n );
            return 0;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int _accept4( int sockfd, struct sockaddr *addr, socklen_t *len, int flags )
    {
        using Address = __dios::fs::Socket::Address;

        if ( addr && !len )
            throw Error( EFAULT );

        if (( flags | SOCK_NONBLOCK | SOCK_CLOEXEC ) != ( SOCK_NONBLOCK | SOCK_CLOEXEC ))
            throw Error( EINVAL );

        Address address;
        int newSocket = instance( ).accept( sockfd, address );

        if ( addr ) {
            struct sockaddr_un *target = reinterpret_cast< struct sockaddr_un * >( addr );
            target->sun_family = AF_UNIX;
            char *end = std::copy( address.value( ).begin( ), address.value( ).end( ), target->sun_path );
            *end = '\0';
            *len = address.size( ) + 1 + sizeof( target->sun_family );
        }
        if ( flags & SOCK_NONBLOCK )
            instance().getFile( newSocket )->flags() |= O_NONBLOCK;

        return newSocket;
    }

    int accept( int sockfd, struct sockaddr *addr, socklen_t *len )
    {
        try {
            return _accept4( sockfd, addr, len, 0 );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int accept4( int sockfd, struct sockaddr *addr, socklen_t *len, int flags )
    {
        try {
            return _accept4( sockfd, addr, len, flags );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int _renameitemat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath )
    {
        instance( ).renameAt( newdirfd, newpath, olddirfd, oldpath );
        return 0;
    }

    int renameat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath )
    {
        try {
            return _renameitemat(olddirfd, oldpath, newdirfd, newpath );
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return -1;
        }
    }

    int rename( const char *oldpath, const char *newpath )
    {
        return renameat( AT_FDCWD, oldpath, AT_FDCWD, newpath );
    }

    //char *getcwd(char *buf, size_t size);
    char *getcwd( char *buff, size_t size ) {
        try {
            instance( ).getCurrentWorkingDir( buff, size );
            return buff;
        } catch ( Error & e ) {
            *__dios_errno() = e.code();
            return nullptr;
        }
    }

private:
    Manager *_manager;
};

} // namespace fs
} // namespace __dios

#endif
