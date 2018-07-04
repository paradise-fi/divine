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

#include <dios/vfs/syscall.hpp>

namespace __dios::fs
{

    int Syscall::fchmodat( int dirfd, const char *path, Mode mode, int flags )
    {
        if ( ( flags | AT_SYMLINK_NOFOLLOW ) != AT_SYMLINK_NOFOLLOW )
            return error( EINVAL ), -1;

        if ( auto ino = lookup( get_dir( dirfd ), path, !( flags & AT_SYMLINK_NOFOLLOW ) ) )
            return _chmod( ino, mode ), 0;
        else
            return -1;
    }

    int Syscall::fchmod( int fd_, Mode mode )
    {
        if ( auto fd = check_fd( fd_, W_OK ) )
            return _chmod( fd->inode(), mode ), 0;
        else
            return -1;
    }

    int Syscall::chmod( const char *path, Mode mode )
    {
        return fchmodat( AT_FDCWD, path, mode, 0 );
    }

}
