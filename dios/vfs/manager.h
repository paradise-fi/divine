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
    std::shared_ptr< FileDescriptor > getFile( int fd );
    std::shared_ptr< Socket > getSocket( int sockfd );

    template< typename U > friend struct VFS;
    Node _root;

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

    using Syscall::dup;
    using Syscall::dup2;
    using Syscall::fcntl;

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
    using Syscall::lseek;

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

    using Syscall::pipe;
    using Syscall::socketpair;
    using Syscall::socket;

    using Syscall::getsockname;
    using Syscall::getpeername;

    using Syscall::connect;
    using Syscall::listen;
    using Syscall::bind;
    using Syscall::accept;
    using Syscall::accept4;

    using Syscall::send;
    using Syscall::sendto;

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< VFS >( "{VFS}" );

        for ( auto env = s.env ; env->key; env++ )
            if ( !strcmp( env->key, "vfs.stdin" ) )
                _stdio[ 0 ] = fs::make_shared< StandardInput >( env->value, env->size );

        if ( !_stdio[ 0 ] )
            _stdio[ 0 ] = fs::make_shared< StandardInput >();
        _stdio[ 1 ] = make_tracefile( s.opts, "stdout" );
        _stdio[ 2 ] = make_tracefile( s.opts, "stderr" );

        for ( int i = 0; i < 2; ++i )
            _stdio[ i ]->mode( S_IFREG | S_IRUSR );

        auto &fds = s.proc1->_openFD;
        fds.resize( 3 );
        for ( int i = 0; i < 2; ++i )
            fds[ i ] = fs::make_shared< FileDescriptor >( _stdio[ i ], i ? O_WRONLY : O_RDONLY );

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

    static Node make_tracefile( SysOpts& o, String stream )
    {
        auto r = std::find_if( o.begin(), o.end(), [&]( const auto& o ) { return o.first == stream; } );

        if ( r == o.end() || r->second == "trace" )
            return make_shared< VmBuffTraceFile >();
        if ( r->second == "unbuffered" )
            return make_shared< VmTraceFile >();
        if ( r->second == "notrace" )
            return nullptr;

        __dios_trace_f( "Invalid configuration for file %s", stream.c_str() );
        __dios_fault( _DiOS_F_Config, "Invalid file tracing configuration" );
        __builtin_trap();
    }

private: /* helper methods */

    Node _root;
    std::array< Node, 3 > _stdio;

public: /* system call implementation */

    int creat( const char *path, Mode mode )
    {
        return mknodat( AT_FDCWD, path, mode | S_IFREG, 0 );
    }

    int close( int fd )
    {
        if ( check_fd( fd, F_OK ) )
            return proc()._openFD[ fd ].reset(), 0;
        else
            return -1;
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

private:
    Manager *_manager;
};

} // namespace fs
} // namespace __dios

#endif
