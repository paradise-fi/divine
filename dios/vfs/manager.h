// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "inode.hpp"
#include "flags.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "fd.hpp"
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

    void closeFile( int fd );
    int duplicate( int oldfd, int lowEdge = 0 );
    int duplicate2( int oldfd, int newfd );
    std::shared_ptr< FileDescriptor > getFile( int fd );
    std::shared_ptr< Socket > getSocket( int sockfd );

    std::pair< int, int > pipe();

    off_t lseek( int fd, off_t offset, Seek whence );

    int socket( SocketType type, OFlags fl );
    std::pair< int, int > socketpair( SocketType type, OFlags fl );
    int accept( int sockfd, Socket::Address &address );

    template< typename U > friend struct VFS;
    Node _root;
    std::array< Node, 3 > _standardIO;

    ProcessInfo *_proc;

    Manager( bool );// private default ctor

    int _getFileDescriptor( Node node, OFlags flags, int lowEdge = 0 );
    int _getFileDescriptor( std::shared_ptr< FileDescriptor >, int lowEdge = 0 );
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
    ProcessInfo &proc() override { return *static_cast< Process * >( this->getCurrentTask()->_proc ); }

    using Syscall::open;
    using Syscall::openat;

    using Syscall::chmod;
    using Syscall::fchmod;
    using Syscall::fchmodat;

    using Syscall::read;
    using Syscall::write;

    using Syscall::mkdir;
    using Syscall::mkdirat;
    using Syscall::mknod;
    using Syscall::mknodat;

    using Syscall::truncate;
    using Syscall::ftruncate;

    using Syscall::unlink;
    using Syscall::rmdir;
    using Syscall::unlinkat;

    using Syscall::fstatat;
    using Syscall::stat;
    using Syscall::lstat;
    using Syscall::fstat;

    using Syscall::linkat;
    using Syscall::link;
    using Syscall::symlinkat;
    using Syscall::symlink;
    using Syscall::readlinkat;
    using Syscall::readlink;

    using Syscall::umask;

    using Syscall::renameat;
    using Syscall::rename;

    using Syscall::chdir;
    using Syscall::fchdir;
    using Syscall::getcwd;

    using Syscall::faccessat;
    using Syscall::access;

    using Syscall::connect;
    using Syscall::bind;

    template< typename Setup >
    void setup( Setup s ) {
        traceAlias< VFS >( "{VFS}" );

        auto &sio = instance()._standardIO;

        for ( auto env = s.env ; env->key; env++ )
            if ( !strcmp( env->key, "vfs.stdin" ) )
                sio[ 0 ].reset( new( __dios::nofail ) StandardInput( env->value, env->size ) );

        instance().setOutputFile( getFileTraceConfig( s.opts, "stdout" ) );
        instance().setErrFile( getFileTraceConfig( s.opts, "stderr" ) );

        for ( int i = 0; i < 2; ++i )
            sio[ i ]->mode( S_IFREG | S_IRUSR );

        s.proc1->_openFD = __dios::Vector< std::shared_ptr< FileDescriptor > > ( {
        std::allocate_shared< FileDescriptor >( __dios::AllocatorPure(), sio[ 0 ], O_RDONLY ),
        std::allocate_shared< FileDescriptor >( __dios::AllocatorPure(), sio[ 1 ], O_WRONLY ),
        std::allocate_shared< FileDescriptor >( __dios::AllocatorPure(), sio[ 2 ], O_WRONLY )
        } );
        s.proc1->_umask = S_IWGRP | S_IWOTH;

        _root = instance()._root;
        s.proc1->_cwd = _root;

        import( s.env );
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

private:
    Manager *_manager;
};

} // namespace fs
} // namespace __dios

#endif
