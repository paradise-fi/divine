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

    ssize_t Syscall::write( int fd_, const void *buf, size_t count )
    {
        if ( auto fd = check_fd( fd_, W_OK ) )
            return fd->write( buf, count );
        else
            return -1;
    }

    ssize_t Syscall::read( int fd_, void* buf, size_t count )
    {
        if ( auto fd = check_fd( fd_, R_OK ) )
            return fd->read( buf, count );
        else
            return -1;
    }

    int Syscall::mkdirat( int dirfd, const char *path, Mode mode )
    {
        if ( link_node( get_dir( dirfd ), path, new_node( mode | S_IFDIR ), true ) )
            return 0;
        else
            return -1;
    }

    int Syscall::mkdir( const char *path, Mode mode )
    {
        return mkdirat( AT_FDCWD, path, mode );
    }

    int Syscall::mknodat( int dirfd, const char *path, Mode mode, dev_t dev )
    {
        if ( dev != 0 )
            return error( EINVAL ), -1;
        if ( !mode.is_char() && !mode.is_block() && !mode.is_file() && !mode.is_fifo() &&
             !mode.is_socket() )
            return error( EINVAL ), -1;

        if ( link_node( get_dir( dirfd ), path, new_node( mode ), true ) )
            return 0;
        else
            return -1;
    }

    int Syscall::mknod( const char *path, Mode mode, dev_t dev )
    {
        return mknodat( AT_FDCWD, path, mode, dev );
    }

    int Syscall::ftruncate( int fd_, off_t length )
    {
        if ( auto fd = check_fd( fd_, W_OK ) )
            return _truncate( fd->inode(), length );

        if ( !check_fd( fd_, F_OK ) )
            return -1;

        return error( EINVAL ), -1; /* also override EBADF from check_fd */
    }

    int Syscall::truncate( const char *path, off_t length )
    {
        if ( auto ino = lookup( get_dir(), path, true ) )
            return _truncate( ino, length );
        else
            return -1;
    }

    int Syscall::open( const char *path, int flags, Mode mode )
    {
        return openat( AT_FDCWD, path, flags, mode );
    }

    int Syscall::openat( int dirfd, const char *path, OFlags flags, Mode mode )
    {
        auto ino = lookup( get_dir( dirfd ), path, flags.follow() );

        if ( ino && flags.has( O_CREAT ) && flags.has( O_EXCL ) )
            return error( EEXIST ), -1;

        if ( !ino && flags.has( O_CREAT ) )
            if ( !link_node( get_dir( dirfd ), path, ino = new_node( mode | S_IFREG ), flags.follow() ) )
                return -1;

        if ( !ino )
            return -1;

        /* FIXME check the uid &c. */

        if ( ( flags.read() || flags.noaccess() ) && !ino->mode().user_read() )
            return error( EACCES ), -1;

        if ( flags.write() || flags.noaccess() )
        {
            if ( !ino->mode().user_write() )
                return error( EACCES ), -1;
            if ( ino->mode().is_dir())
                return error( EISDIR ), -1;
        }

        if ( flags.has( O_DIRECTORY ) && !ino->mode().is_dir() )
            return error( ENOTDIR ), -1;

        return new_fd( ino, flags );
    }

    int Syscall::unlink( const char *path )
    {
        return unlinkat( AT_FDCWD, path, 0 );
    }

    int Syscall::rmdir( const char *path )
    {
        return unlinkat( AT_FDCWD, path, AT_REMOVEDIR );
    }

    int Syscall::unlinkat( int dirfd, const char *path, int flags )
    {
        auto [ dir, name ] = lookup_dir( get_dir( dirfd ), path, true );
        if ( !dir )
            return -1;

        auto ino = lookup( dir, name, false );
        if ( !ino )
            return -1;

        if ( flags == AT_REMOVEDIR )
        {
            if ( !ino->mode().is_dir() )
                return error( ENOTDIR ), -1;
            if ( !ino->as< Directory >()->empty() )
                return error( ENOTEMPTY ), -1;
        }

        if ( flags == 0 && ino->mode().is_dir() )
            return error( EISDIR ), -1;

        return dir->as< Directory >()->unlink( name ) ? 0 : -1;
    }

}
