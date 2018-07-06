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

#include <dios/vfs/inode.h>
#include <dios/vfs/descriptor.h>
#include <dios/vfs/directory.h>
#include <dios/vfs/file.h> /* for symlink */

#pragma once

namespace __dios::fs
{
    struct ProcessInfo
    {
        Mode _umask;
        __dios::Vector< std::shared_ptr< FileDescriptor > > _openFD;
    };

    struct Base
    {
        virtual ProcessInfo &proc() = 0;
        virtual Node root() = 0;
        virtual Node cwd() = 0;

        FileDescriptor *get_fd( int fd )
        {
            auto &open = proc()._openFD;
            if ( fd >= 0 && fd < int( open.size() ) && open[ fd ] )
                return open[ fd ].get();
            return nullptr;
        }

        int new_fd( Node n, OFlags flags, int min = 0 )
        {
            auto &open = proc()._openFD;
            int fd;

            if ( min >= int( open.size() ) )
                open.resize( min + 1 );

            for ( fd = min; fd < int( open.size() ); ++ fd )
                if ( !get_fd( fd ) )
                    break;

            if ( fd == int( open.size() ) )
                open.emplace_back();

            open[ fd ] = fs::make_shared< FileDescriptor >( n, flags );
            return fd;
        }

        Node new_node( Mode mode )
        {
            Node r;

            if ( mode.is_socket() )
                r = make_shared< SocketDatagram >();

            if ( mode.is_file() )
                r = make_shared< RegularFile >();

            if ( mode.is_dir() )
                r = make_shared< Directory >();

            if ( mode.is_fifo() )
                r = make_shared< Pipe >();

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
                return cwd();
            else
                return get_fd( fd )->inode();
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

        Node search( Node dir, std::string_view path, bool follow )
        {
            if ( path.empty() )
                return dir;

            if ( !dir->mode().is_dir() )
                return error( ENOTDIR ), nullptr;

            auto [name, tail] = split( path, '/' );
            if ( name.size() > FILE_NAME_LIMIT )
                return error( ENAMETOOLONG ), nullptr;

            if ( auto next = dir->template as< Directory >()->find( name ) )
            {
                if ( auto link = next->template as< SymLink >(); link && follow )
                    return search( lookup( dir, link->target(), follow ), tail, follow );

                return search( next, tail, follow );
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
                return search( dir, path, follow );
        }

        bool link_node( Node dir, std::string_view path, Node ino, bool follow )
        {
            if ( path[0] == '/' )
                return link_node( root(), path.substr( 1, String::npos ), ino, follow );

            auto [parent_path, name] = split( path, '/', true );
            auto parent = lookup( dir, parent_path, follow );
            if ( !parent || !parent->mode().is_dir() )
            {
                __dios_trace_t( "parent is not a dir" );
                return false;
            }

            __dios_trace_t( "parent ok, calling create" );
            return parent->as< Directory >()->create( name, ino );
        }

    };
}
