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

    off_t Syscall::lseek( int fd_, off_t offset, int whence )
    {
        auto fd = check_fd( fd_, F_OK );
        if ( !fd )
            return -1;

        if ( fd->inode()->mode().is_fifo() )
            return error( ESPIPE ), -1;

        auto size_t_max = std::numeric_limits< size_t >::max();

        switch ( whence )
        {
            case SEEK_SET:
                if ( offset < 0 )
                    return error( EINVAL ), -1;
                fd->offset( offset );
                break;

            case SEEK_CUR:
                if ( offset > 0 && size_t_max - fd->offset() < size_t( offset ) )
                    return error( EOVERFLOW ), -1;
                if ( int( fd->offset() ) < -offset )
                    return error( EINVAL ), -1;
                fd->offset( offset + fd->offset() );
                break;

            case SEEK_END:
                if ( offset > 0 && size_t_max - fd->size() < size_t( offset ) )
                    return error( EOVERFLOW ), -1;
                if ( offset < 0 && int( fd->size() ) < -offset )
                    return error( EINVAL ), -1;
                fd->offset( fd->size() + offset );
                break;

            default:
                return error( EINVAL ), -1;
        }

        return fd->offset();
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

    int Syscall::_dup( int fd_, int min )
    {
        if ( auto fd = check_fd( fd_, F_OK ) )
        {
            int newfd = new_fd( nullptr, 0, min );
            proc()._openFD[ newfd ] = fd;
            return newfd;
        }
        else
            return -1;
    }

    int Syscall::dup2( int fd_, int newfd )
    {
        if ( newfd < 0 || newfd > FILE_DESCRIPTOR_LIMIT )
            return error( EBADF ), -1;
        if ( int( proc()._openFD.size() ) <= newfd )
            proc()._openFD.resize( newfd + 1 );
        auto &fd = proc()._openFD[ newfd ] = check_fd( fd_, F_OK );
        return fd ? newfd : -1;
    }

    int Syscall::fcntl( int fd_, int cmd, va_list *vl )
    {
        auto fd = check_fd( fd_, F_OK );
        if ( !fd )
            return -1;

	switch ( cmd )
        {
	    case F_SETFD:
	    case F_GETFD:
		va_end( *vl );
		return 0;
	    case F_DUPFD_CLOEXEC: // for now let assume we cannot handle O_CLOEXEC
		va_end( *vl );
		return -1;
	    case F_DUPFD:
	    {
		int min = va_arg(  *vl, int );
		va_end( *vl );
		return _dup( fd_, min );
	    }
	    case F_GETFL:
		va_end( *vl );
		return fd->flags().to_i();
	    case F_SETFL:
	    {
		OFlags mode = va_arg( *vl, int );

		if ( mode & ~( O_APPEND | O_NONBLOCK ) )
		    return error( EINVAL ), -1;
		if ( !( mode & O_APPEND ) && ( fd->flags() & O_APPEND ) )
		    return error( EPERM ), -1;

		fd->flags() &= ~( O_APPEND | O_NONBLOCK );
		fd->flags() |= mode;

		va_end( *vl );
		return 0;
	    }
	    default:
		__dios_trace_f( "the fcntl command %d is not implemented", cmd );
		va_end( *vl );
		return -1;
	}
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

    int Syscall::symlinkat( const char *target_, int dirfd, const char *linkpath )
    {
        std::string_view target( target_ );

        if ( target.size() > PATH_LIMIT )
            return error( ENAMETOOLONG ), -1;

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
        return _chdir( lookup( get_dir(), path, true ) );
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

    int Syscall::socketpair( int dom, int type, int proto, int fds[2] )
    {
        if ( dom != AF_UNIX )
            return error( EAFNOSUPPORT ), -1;

        if ( type != SOCK_STREAM )
            return error( EPROTOTYPE ), -1;

        if ( proto ) /* TODO: support SOCK_NONBLOCK */
            return error( EOPNOTSUPP ), -1;

        auto ino_a = make_shared< SocketStream >(), ino_b = make_shared< SocketStream >();
        ino_a->connect( ino_a, ino_b, false );
        ino_a->mode( ACCESSPERMS | S_IFSOCK );
        ino_b->mode( ACCESSPERMS | S_IFSOCK );

        fds[0] = new_fd( ino_a, O_RDWR );
        fds[1] = new_fd( ino_b, O_RDWR );
        return 0;
    }

    int Syscall::pipe( int fds[2] )
    {
        auto ino = make_shared< Pipe >();
        ino->mode( S_IRWXU | S_IFIFO );

        fds[0] = new_fd( ino, O_RDONLY | O_NONBLOCK );
        fds[1] = new_fd( ino, O_WRONLY | O_NONBLOCK );
        return 0;
    }

    int Syscall::socket( int domain, SFlags t, int protocol )
    {
        if ( domain != AF_UNIX )
            return error( EAFNOSUPPORT ), -1;
        if ( protocol )
            return error( EPROTONOSUPPORT ), -1;

        Node ino;

        switch ( t.type() )
        {
            case SOCK_STREAM:
                ino = make_shared< SocketStream >(); break;
            case SOCK_DGRAM:
                ino = make_shared< SocketDatagram >(); break;
            default:
                return error( EPROTONOSUPPORT ), -1;
        }

        ino->mode( ACCESSPERMS | S_IFSOCK );
        return new_fd( ino, O_RDWR | ( t & SOCK_NONBLOCK ? O_NONBLOCK : 0 ) );
    }

    int Syscall::getsockname( int sockfd, struct sockaddr *addr, socklen_t *len )
    {
        auto fd = check_fd( sockfd, F_OK );
        if ( address_out( fd->inode(), addr, len ) )
            return 0;
        else
            return -1;
    }

    int Syscall::getpeername( int sockfd, struct sockaddr *addr, socklen_t *len )
    {
        auto fd = check_fd( sockfd, F_OK );
        if ( address_out( fd->inode()->peer(), addr, len ) )
            return 0;
        else
            return -1;
    }

    int Syscall::connect( int sockfd, const struct sockaddr *addr, socklen_t len )
    {
        auto [ ino, path ] = get_sock( sockfd, addr, len );
        if ( !ino )
            return -1;

        if ( auto remote = lookup( get_dir(), path, true ) )
        {
            if ( !remote->mode().user_read() )
                return error( EACCES ), -1;
            if ( ino->connect( ino, remote ) )
                return 0;
        }

        return -1;
    }

    int Syscall::listen( int sockfd, int n )
    {
        if ( auto fd = check_fd( sockfd, F_OK ) )
            return fd->inode()->listen( n ) ? 0 : -1;
        else
            return -1;
    }

    int Syscall::bind( int sockfd, const struct sockaddr *addr, socklen_t len )
    {
        if ( addr->sa_family != AF_UNIX )
            return error( EINVAL ), -1;

        auto [ ino, path ] = get_sock( sockfd, addr, len );
        if ( !ino )
            return -1;

        auto [ dir, name ] = lookup_dir( get_dir(), path, true );
        if ( !dir )
            return -1;

        if ( !link_node( dir, name, ino, false ) )
            return -1;

        if ( ino->bind( path ) )
            return 0;
        else
            return -1;
    }

    int Syscall::accept( int fd, struct sockaddr *addr, socklen_t *len )
    {
        return accept4( fd, addr, len, 0 );
    }

    int Syscall::accept4( int sockfd, struct sockaddr *addr, socklen_t *len, SFlags flags )
    {
        if ( ( flags | SOCK_NONBLOCK ) != SOCK_NONBLOCK )
            return error( EINVAL ), -1;

        auto sock = check_fd( sockfd, F_OK );
        OFlags fd_flags;

        if ( !sock )
            return -1;

        if ( flags & SOCK_NONBLOCK )
            fd_flags |= O_NONBLOCK;

        auto ino = sock->inode()->accept();
        if ( !ino )
            return -1;

        if ( addr && !address_out( ino, addr, len ) )
            return -1;

        return new_fd( ino, fd_flags );
    }

    ssize_t Syscall::send( int sockfd, const void *buf, size_t n, int )
    {
        if ( auto sock = check_fd( sockfd, W_OK ) )
            return sock->write( buf, n );
        else
            return -1;
    }

    ssize_t Syscall::sendto( int sockfd, const void *buf, size_t n, int flags,
                             const sockaddr *addr, socklen_t len )
    {
        if ( !addr )
            return send( sockfd, buf, n, flags );

        auto [ ino, path ] = get_sock( sockfd, addr, len );
        if ( !ino )
            return -1;

        if ( auto remote = lookup( get_dir(), path, true ) )
            return check_fd( sockfd, F_OK )->write( buf, n, remote );
        else
            return -1;
    }

    ssize_t Syscall::recv( int sockfd, void *buf, size_t n, MFlags flags )
    {
        return recvfrom( sockfd, buf, n, flags, nullptr, nullptr );
    }

    ssize_t Syscall::recvfrom( int sockfd, void *buf, size_t n, MFlags flags,
                               struct sockaddr *addr, socklen_t *len )
    {
        auto fd = check_fd( sockfd, F_OK );
        if ( !fd )
            return -1;

        if ( fd->flags().nonblock() && !fd->inode()->canRead() )
            return error( EAGAIN ), -1;

        if ( fd->flags().nonblock() )
            flags |= MSG_DONTWAIT;

        if ( auto peer = fd->inode()->receive( static_cast< char * >( buf ), n, flags ) )
        {
            if ( addr && !address_out( peer, addr, len ) )
                return -1;
            else
                return n;
        }

        return -1;
    }

}
