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
#include "syscall.hpp"

#ifndef _FS_MANAGER_H_
#define _FS_MANAGER_H_

namespace __dios {
namespace fs {

template < typename Next >
struct VFS: Syscall, Next
{
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
    using Syscall::recv;
    using Syscall::recvfrom;

    void sysfork( pid_t *child )
    {
        Next::sysfork( child );
        /* FIXME the shared pointers in _openFD are probably all mangled */
        Process *proc = static_cast< Process * >( Next::findProcess( *child ) );
        if ( proc )
            for ( auto &fd : proc->_openFD )
                if ( fd && fd->inode() )
                    fd->inode()->open();
        return proc;
    }

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< VFS >( "{VFS}" );

        for ( auto env = s.env ; env->key; env++ )
            if ( !strcmp( env->key, "vfs.stdin" ) )
                _stdio[ 0 ] = new ( nofail ) StandardInput( env->value, env->size );

        if ( !_stdio[ 0 ] )
            _stdio[ 0 ] = new ( nofail ) StandardInput();
        _stdio[ 1 ] = make_tracefile( s.opts, "stdout" );
        _stdio[ 2 ] = make_tracefile( s.opts, "stderr" );

        for ( int i = 0; i < 2; ++i )
            _stdio[ i ]->mode( S_IFREG | S_IRUSR );

        auto &fds = s.proc1->_openFD;
        fds.resize( 3 );
        for ( int i = 0; i < 2; ++i )
            fds[ i ] = fs::make_shared< FileDescriptor >( _stdio[ i ], i ? O_WRONLY : O_RDONLY );

        s.proc1->_umask = S_IWGRP | S_IWOTH;

        _root = new ( nofail ) Directory();
        _root->mode( S_IFDIR | ACCESSPERMS );
        _root->link();
        s.proc1->_cwd = _root;

        import( s.env );
        Next::setup( s );
    }

     void getHelp( Map< std::string_view, HelpOption >& options )
     {
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

    static Node make_tracefile( SysOpts& o, String stream )
    {
        auto r = std::find_if( o.begin(), o.end(), [&]( const auto& o ) { return o.first == stream; } );

        if ( r == o.end() || r->second == "trace" )
            return new ( nofail ) VmBuffTraceFile();
        if ( r->second == "unbuffered" )
            return new ( nofail ) VmTraceFile();
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
};

} // namespace fs
} // namespace __dios

#endif
