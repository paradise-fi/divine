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
#include <sys/un.h>

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
        if ( link_node( get_dir( dirfd ), path, new_node( mode | S_IFDIR ) ) )
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

        if ( link_node( get_dir( dirfd ), path, new_node( mode ) ) )
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
            if ( !link_node( get_dir( dirfd ), path, ino = new_node( mode | S_IFREG ) ) )
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

    int Syscall::fstatat( int dirfd, const char *path, struct stat *buf, int flag )
    {
        auto [ dir, name ] = lookup_dir( get_dir( dirfd ), path, true );
        if ( !dir )
            return -1;
        if ( auto ino = lookup( dir, name, !( flag & AT_SYMLINK_NOFOLLOW ) ) )
            return ino->stat( *buf ), 0;
        else
            return -1;
    }

    int Syscall::stat( const char *path, struct stat *buf )
    {
        return fstatat( AT_FDCWD, path, buf, 0 );
    }

    int Syscall::lstat( const char *path, struct stat *buf  )
    {
        return fstatat( AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW );
    }

    int Syscall::fstat( int fd_, struct stat *buf )
    {
        if ( auto fd = check_fd( fd_, F_OK ) )
            return fd->inode()->stat( *buf ), 0;
        else
            return -1;
    }

    int Syscall::linkat( int odirfd, const char *target, int ndirfd, const char *link, int flags )
    {
        if ( ( flags | AT_SYMLINK_FOLLOW ) != AT_SYMLINK_FOLLOW )
            return error( EINVAL ), -1;

        if ( auto ino = lookup( get_dir( odirfd ), target, flags & AT_SYMLINK_FOLLOW ) )
        {
            if ( ino->mode().is_dir() )
                return error( EPERM ), -1;
            if ( link_node( get_dir( ndirfd ), link, ino ) )
                return 0;
        }

        return -1;
    }

    int Syscall::link( const char *target, const char *linkpath )
    {
        return linkat( AT_FDCWD, target, AT_FDCWD, linkpath, 0 );
    }

    int Syscall::symlinkat( const char *target, int dirfd, const char *linkpath )
    {
        auto ino = fs::make_shared< SymLink >( target );
        ino->mode( ACCESSPERMS | S_IFLNK );
        if ( link_node( get_dir( dirfd ), linkpath, ino ) )
            return 0;
        else
            return -1;
    }

    int Syscall::symlink( const char *target, const char *linkpath )
    {
        return symlinkat( target, AT_FDCWD, linkpath );
    }

    int Syscall::renameat( int odirfd, const char *opath, int ndirfd, const char *npath )
    {
        auto [ odir, oname ] = lookup_dir( get_dir( odirfd ), opath, true );
        auto [ ndir, nname ] = lookup_dir( get_dir( ndirfd ), npath, true );

        if ( !odir || !ndir )
            return error( ENOENT ), -1;

        auto oino = lookup( odir, oname, false ), nino = lookup( ndir, nname, false );
        if ( !oino )
            return error( ENOENT ), -1;

        if ( !odir->mode().user_write() || !ndir->mode().user_write() )
            return error( EACCES ), -1;

        if ( oino == nino )
            return error( EINVAL ), -1;

        if ( nino )
        {
            if ( oino->mode().is_dir() && !nino->mode().is_dir() )
                return error( ENOTDIR ), -1;

            if ( !oino->mode().is_dir() && nino->mode().is_dir() )
                return error( EISDIR ), -1;

            if ( nino->mode().is_dir() && nino->size() > 2 )
                return error( ENOTEMPTY ), -1;
        }

        if ( !odir->as< Directory >()->unlink( oname ) )
            __builtin_trap();

        if ( !link_node( ndir, nname, oino, true ) )
            __builtin_trap();

        return 0;
    }

    bool rappend( char *&buff, size_t &size, std::string_view add )
    {
        if ( size <= add.size() )
            return false;
        std::copy( add.rbegin(), add.rend(), buff );
        buff += add.size();
        size -= add.size();
        *buff = 0;
        return true;
    }

    char *Syscall::getcwd( char *buff, size_t size )
    {
        auto buff_start = buff;

        if ( !buff )
            return error( EFAULT ), nullptr;
        if ( !size )
            return error( EINVAL ), nullptr;

        for ( auto dir = proc()._cwd; dir != root(); dir = dir->as< Directory >()->parent() )
        {
            if ( !rappend( buff, size, dir->as< Directory >()->name() ) )
                return error( ERANGE ), nullptr;

            if ( !rappend( buff, size, "/" ) )
                return error( ERANGE ), nullptr;
        }

        if ( proc()._cwd == root() )
            if ( !rappend( buff, size, "/" ) )
                return error( ERANGE ), nullptr;

        for ( auto c = buff_start; c < buff; ++c )
            std::swap( *c, *--buff );

        return buff_start;
    }

    int Syscall::chdir( const char *path )
    {
        return _chdir( lookup( get_dir( AT_FDCWD ), path, true ) );
    }

    int Syscall::fchdir( int dirfd )
    {
        return _chdir( get_dir( dirfd ) );
    }

    int Syscall::access( const char *path, int amode )
    {
        return faccessat( AT_FDCWD, path, amode, 0 );
    }

    int Syscall::faccessat( int dirfd, const char *path, int mode, int flag )
    {
        if ( ( flag | AT_EACCESS ) != AT_EACCESS )
            return error( EINVAL ), -1;

        if ( ( mode | R_OK | W_OK | X_OK ) != ( R_OK | W_OK | X_OK ) )
            return error( EINVAL ), -1;

        if ( auto ino = lookup( get_dir( dirfd ), path, true ) )
        {
            if ( ( ( mode & R_OK ) && !ino->mode().user_read() ) ||
                 ( ( mode & W_OK ) && !ino->mode().user_write() ) ||
                 ( ( mode & X_OK ) && !ino->mode().user_exec() ) )
                return error( EACCES ), -1;
            else
                return 0;
        }
        else
            return -1;
    }

    ssize_t Syscall::readlink( const char *path, char *buf, size_t size )
    {
        return readlinkat( AT_FDCWD, path, buf, size );
    }

    ssize_t Syscall::readlinkat( int dirfd, const char *path, char *buf, size_t size )
    {
        if ( auto ino = lookup( get_dir( dirfd ), path, false ) )
        {
            if ( !ino->mode().is_link() )
                return error( EINVAL ), -1;
            auto tgt = ino->as< SymLink >()->target();
            size = std::min( tgt.size(), size );
            std::copy( tgt.begin(), tgt.begin() + size, buf );
            return size;
        }
        else
            return error( ENOENT ), -1;
    }

    int Syscall::connect( int sockfd, const struct sockaddr *addr, socklen_t )
    {
        auto sock = check_fd( sockfd, W_OK );
        if ( !sock )
            return -1;
        if ( !addr )
            return error( EFAULT ), -1;
        if ( addr->sa_family != AF_UNIX )
            return error( EAFNOSUPPORT ), -1; /* FIXME */

        auto un = reinterpret_cast< const sockaddr_un * >( addr );
        /* FIXME check the size of addr against len? */
        if ( auto ino = lookup( get_dir(), un->sun_path, true ) )
        {
            if ( !ino->mode().user_read() )
                return error( EACCES ), -1;
            sock->inode()->connect( sock->inode(), ino );
            return 0;
        }

        return -1;
    }

}
