// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018, 2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <tuple>
#include <memory>
#include <algorithm>
#include <utility>
#include <array>
#include <sys/stat.h>
#include <sys/socket.h>
#include <bits/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dios.h>
#include <sys/vmutil.h>
#include <string_view>

#include "inode.hpp"
#include "flags.hpp"
#include "file.hpp"
#include "directory.hpp"
#include "fd.hpp"
#include "syscall.hpp"

#include <dios/sys/options.hpp>

namespace __dios::fs
{

    /* This is a part of the DiOS config stack which provides filesystem
       functionality.

       - Extends process-local information (struct Process) with filesystem
         data.
       - Overrideds fork to keep files open.
       - In setup initializes stdin (empty, or with a specified content if
         passed to it), creates stdout & stder, and imports filesystem
         snapshot. */
    template < typename Next >
    struct VFS: Syscall, Next
    {
        struct Process : Next::Process, fs::ProcessInfo
        {};

        Node root() override { return _root; }
        ProcessInfo &proc() override { return *static_cast< Process * >( this->current_process() ); }

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
        using Syscall::__getcwd;

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
            Process *proc = static_cast< Process * >( Next::findProcess( *child ) );
            if ( proc )
                for ( auto &fd : proc->_fds )
                    if ( fd.inode() )
                        fd.inode()->open(); /* update refcount */
            return proc;
        }

        template< typename Setup >
        void setup( Setup s )
        {
            traceAlias< VFS >( "{VFS}" );

            for ( auto env = s.env ; env->key; env++ )
                if ( !strcmp( env->key, "vfs.stdin" ) )
                    _stdio[ 0 ] = new StandardInput( env->value, env->size );

            if ( !_stdio[ 0 ] )
                _stdio[ 0 ] = new StandardInput();
            _stdio[ 1 ] = make_tracefile( s.opts, "stdout" );
            _stdio[ 2 ] = make_tracefile( s.opts, "stderr" );

            for ( int i = 0; i <= 2; ++i )
            {
                _stdio[ i ]->mode( S_IFREG | S_IRUSR );
                s.proc1->_fds.emplace_back( _stdio[ i ], i ? O_WRONLY : O_RDONLY );
                s.proc1->_fd_refs.emplace_back( i );
            }

            s.proc1->_umask = S_IWGRP | S_IWOTH;

            _root = new Directory();
            _root->mode( S_IFDIR | ACCESSPERMS );
            _root->link();
            s.proc1->_cwd = _root;

            import( s.env );
            Next::setup( s );
        }

         void getHelp( ArrayMap< std::string_view, HelpOption >& options )
         {
            const char *opt1 = "{stdout|stderr}";
            if ( options.find( opt1 ) != options.end() )
            {
                __dios_trace_f( "Option %s already present", opt1 );
                __dios_fault( _DiOS_F_Config, "Option conflict" );
            };

            options[ { opt1 } ] = { "specify how to treat program output",
              { "notrace - ignore the stream",
                "unbuffered - trace each write",
                "trace - trace after each newline (default)"} };

            Next::getHelp( options );
        }

        static Node make_tracefile( SysOpts& o, std::string_view stream )
        {
            auto r = std::find_if( o.begin(), o.end(),
                                   [&]( const auto& o ) { return o.first == stream; } );

            if ( r == o.end() || r->second == "trace" )
                return new VmBuffTraceFile();
            if ( r->second == "unbuffered" )
                return new VmTraceFile();
            if ( r->second == "notrace" )
                return nullptr;

            __dios_trace_f( "Invalid configuration for file %.*s",
                            int( stream.size() ), stream.begin() );
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

        int close( int fd_ )
        {
            if ( auto fd = check_fd( fd_, F_OK ) )
            {
                fd->unref();
                proc()._fd_refs[ fd_ ] = -1;
                return 0;
            }
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

}
