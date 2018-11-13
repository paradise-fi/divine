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

#include <dios/vfs/inode.hpp>
#include <dios/vfs/fd.hpp>
#include <dios/vfs/directory.hpp>
#include <dios/vfs/file.hpp>
#include <dios/vfs/socket.hpp>
#include <sys/un.h>

#pragma once

namespace __dios::fs
{
    struct ProcessInfo
    {
        Mode _umask;
        Node _cwd;
        Array< short > _fd_refs;
        Array< FileDescriptor > _fds;
    };

    struct Base
    {
        virtual ProcessInfo &proc() = 0;
        virtual Node root() = 0;

        FileDescriptor *get_fd( int fd )
        {
            auto &open = proc()._fd_refs;
            if ( fd >= 0 && fd < int( open.size() ) && open[ fd ] >= 0 )
                return &proc()._fds[ open[ fd ] ];
            return nullptr;
        }

        int new_fd( Node n, OFlags flags, int min = 0 )
        {
            auto &open = proc()._fd_refs;
            int fd;

            if ( min >= int( open.size() ) )
                open.resize( min + 1 );

            for ( fd = min; fd < int( open.size() ); ++ fd )
                if ( !get_fd( fd ) )
                    break;

            if ( fd == int( open.size() ) )
                open.emplace_back();

            if ( n )
            {
                open[ fd ] = proc()._fds.size();
                auto &newfd = proc()._fds.emplace_back( n, flags );
                newfd.ref();
            }

            return fd;
        }

        Node new_node( Mode mode, bool apply_umask = true )
        {
            Node r;

            if ( mode.is_socket() )
                r = new ( nofail ) SocketDatagram();

            if ( mode.is_file() )
                r = new ( nofail ) RegularFile();

            if ( mode.is_dir() )
                r = new ( nofail ) Directory();

            if ( mode.is_fifo() )
                r = new ( nofail ) Pipe();

            if ( mode.is_link() )
                r = new ( nofail ) SymLink();

            if ( apply_umask )
                mode &= ~proc()._umask;

            if ( r )
                r->mode( mode );

            return r;
        }

        FileDescriptor *check_fd( int fd_, int acc )
        {
            auto fd = get_fd( fd_ );
            if ( !fd )
                return error( EBADF ), nullptr;

            auto ino = fd->inode();
            if ( !ino )
                return error( EBADF ), nullptr;

            if ( ( acc & W_OK ) && !fd->flags().write() )
                return error( EBADF ), nullptr;
            if ( ( acc & R_OK ) && !fd->flags().read() )
                return error( EBADF ), nullptr;

            if ( fd->inode()->mode().is_dir() )
                if ( ( acc & R_OK ) && !fd->flags().has( O_DIRECTORY ) )
                    return error( EISDIR ), nullptr;

            return fd;
        }

        Node get_dir( int fd = AT_FDCWD )
        {
            if ( fd == AT_FDCWD )
                return proc()._cwd;
            else if ( auto n = get_fd( fd ) )
                return n->inode();
            else return error( EBADF ), nullptr;
        }

        using split_view = std::pair< std::string_view, std::string_view >;

        split_view split( std::string_view p, char d, bool reverse = false )
        {
            auto npos = std::string::npos;
            auto s = reverse ? p.rfind( d ) : p.find( d );
            if ( s == npos )
                return reverse ? split_view{ "", p } : split_view{ p, "" };
            return { p.substr( 0, s ), p.substr( s + 1, npos ) };
        }

        Node lookup_relative( Node dir, std::string_view path, bool follow, int depth = 8 )
        {
            if ( path.empty() )
                return dir;

            if ( !depth )
                return error( ELOOP ), nullptr;

            if ( !dir->mode().is_dir() )
                return error( ENOTDIR ), nullptr;

            if ( !dir->mode().user_exec() )
                return error( EACCES ), nullptr;

            auto [ name, tail ] = split( path, '/' );
            if ( name.size() > FILE_NAME_LIMIT )
                return error( ENAMETOOLONG ), nullptr;

            if ( auto next = dir->template as< Directory >()->find( name ) )
            {
                if ( next->mode().is_link() )
                {
                    auto link = next->template as< SymLink >();
                    if ( follow || !tail.empty() )
                        return lookup_relative( lookup( dir, link->target(), follow ),
                                                tail, follow, depth - 1 );
                }

                return lookup_relative( next, tail, follow, depth - 1 );
            }
            else
                return error( ENOENT ), nullptr;
        }

        Node lookup( Node dir, std::string_view path, bool follow )
        {
            auto npos = std::string::npos;

            if ( path.size() > PATH_LIMIT )
                return error( ENAMETOOLONG ), nullptr;

            if ( path[0] == '/' )
                return lookup( root(), path.substr( 1, npos ), follow );
            else
                return lookup_relative( dir, path, follow, 8 );
        }

        std::pair< Node, std::string_view > lookup_dir( Node dir, std::string_view path, bool follow )
        {
            auto null = std::make_pair( nullptr, "" );

            if ( path.size() > PATH_LIMIT )
                return error( ENAMETOOLONG ), null;

            if ( !path.empty() && path[0] == '/' )
                return lookup_dir( root(), path.substr( 1, String::npos ), follow );

            auto [ parent_path, name ] = split( path, '/', true );
            auto parent = lookup( dir, parent_path, follow );

            if ( !parent )
                return null;
            if ( !parent->mode().is_dir() )
                return error( ENOTDIR ), null;

            return { parent, name };
        }

        bool link_node( Node dir, std::string_view path, Node ino, bool overwrite = false )
        {
            auto [ parent, name ] = lookup_dir( dir, path, true );

            if ( !parent )
                return false;

            if ( ino->mode().is_dir() )
                ino->as< Directory >()->parent( parent );
            return parent->as< Directory >()->create( name, ino, overwrite );
        }

        std::pair< Node, std::string_view > get_sock( int sockfd, const sockaddr *addr, socklen_t )
        {
            auto null = std::make_pair( nullptr, "" );
            auto sock = check_fd( sockfd, F_OK );

            if ( !sock )
                return null;
            if ( !addr )
                return error( EFAULT ), null;

            if ( addr->sa_family != AF_UNIX )
                return error( EAFNOSUPPORT ), null;

            /* FIXME check the size of addr against len */
            auto un = reinterpret_cast< const sockaddr_un * >( addr );
            return { sock->inode(), un->sun_path };
        }

        bool address_out( Node socket, sockaddr *addr, socklen_t *len )
        {
            if ( !len )
                return error( EFAULT ), false;

            if ( addr )
            {
                auto asun = reinterpret_cast< sockaddr_un * >( addr );
                asun->sun_family = AF_UNIX;
                std::strncpy( asun->sun_path, socket->address().begin(), *len - sizeof( sockaddr ) );
                *len = socket->address().size() + 1 + sizeof( sockaddr );
            }

            return true;
        }

        bool import( const _VM_Env *env, Map< ino_t, Node > & );
        bool import( const _VM_Env *env )
        {
            Map< ino_t, Node > map;
            return import( env, map );
        }
    };
}
