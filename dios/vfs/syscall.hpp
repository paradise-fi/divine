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
    };

}
