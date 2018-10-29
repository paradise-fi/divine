// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <dios/vfs/base.hpp>
#pragma once

namespace __dios::fs
{

    struct Syscall : Base
    {
        using Base::Base;

        int open( const char *path, int flags, Mode mode );
        int openat( int dirfd, const char *path, OFlags flags, Mode mode );

        void _chmod( Node ino, Mode mode )
        {
            ino->mode() = ( ino->mode() & ~ALLPERMS ) | ( mode & ALLPERMS );
        }

        int fchmodat( int dirfd, const char *path, Mode mode, int flags );
        int fchmod( int fd_, Mode mode );
        int chmod( const char *path, Mode mode );

        ssize_t write( int fd_, const void *buf, size_t count );
        ssize_t read( int fd_, void* buf, size_t count );

        int mkdirat( int dirfd, const char *path, Mode mode );
        int mkdir( const char *path, Mode mode );
        int mknodat( int dirfd, const char *path, Mode mode, dev_t dev );
        int mknod( const char *path, Mode mode, dev_t dev );

        int _truncate( Node ino, off_t length )
        {
            if ( length < 0 )
                return error( EINVAL ), -1;
            if ( ino->mode().is_dir() )
                return error( EISDIR ), -1;
            if ( auto file = ino->template as< RegularFile >() )
                return file->resize( length ), 0;
            else
                return error( EINVAL ), -1;
        }

        int ftruncate( int fd_, off_t length );
        int truncate( const char *path, off_t length );

        int unlink( const char *path );
        int rmdir( const char *path );
        int unlinkat( int dirfd, const char *path, int flags );

        int stat( const char *path, struct stat *buf );
        int lstat( const char *path, struct stat *buf  );
        int fstat( int fd, struct stat *buf );
        int fstatat( int dirfd, const char *path, struct stat *buf, int flag );

        int linkat( int olddirfd, const char *target, int newdirfd, const char * linkpath, int flags );
        int link( const char *target, const char *linkpath );
        int symlinkat( const char *target, int dirfd, const char *linkpath );
        int symlink( const char *target, const char *linkpath );

        int umask( Mode mask )
        {
            auto old = proc()._umask;
            proc()._umask = mask & ACCESSPERMS;
            return old.to_i();
        }

        int renameat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath );
        int rename( const char *oldpath, const char *newpath )
        {
            return renameat( AT_FDCWD, oldpath, AT_FDCWD, newpath );
        }

        int _chdir( Node ino )
        {
            if ( !ino )
                return -1;
            if ( !ino->mode().is_dir() )
                return error( ENOTDIR ), -1;
            if ( !ino->mode().user_exec() )
                return error( EACCES ), -1;
            proc()._cwd = ino;
            return 0;
        }

        int chdir( const char *path );
        int fchdir( int dirfd );
        char *getcwd( char *buff, size_t size );

        ssize_t readlinkat( int dirfd, const char *path, char *buf, size_t size );
        ssize_t readlink( const char *path, char *buf, size_t size );

        int access( const char *path, int amode );
        int faccessat( int dirfd, const char *path, int amode, int flag );

        int connect( int sockfd, const struct sockaddr *addr, socklen_t len );
    };

}
