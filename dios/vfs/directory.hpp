// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2015 Jiří Weiser
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

#pragma once

#include <memory>
#include <cerrno>
#include <dirent.h>
#include <string_view>

#include <dios/vfs/inode.hpp>
#include <dios/vfs/flags.hpp>
#include <dios/vfs/utils.h>

namespace __dios::fs
{

struct DirectoryEntry
{
    DirectoryEntry( __dios::String name, Node inode ) :
        _name( std::move( name ) ),
        _inode( std::move( inode ) )
    {}

    __dios::String &name() { return _name; }
    const __dios::String &name() const { return _name; }
    Node inode() const { return _inode; }

private:
    __dios::String _name;
    Node _inode;
};

struct Directory : INode, std::enable_shared_from_this< Directory >
{
    using Items = __dios::Vector< DirectoryEntry >;

    Directory( Node parent = Node{} )
        : _items{}, _parent( parent )
    {}

    size_t size() const override { return _items.size() + 2; }
    bool special_name( std::string_view name ) const { return name == "." || name == ".."; }

    std::string_view name()
    {
        if ( _parent == this )
            return "/";
        for ( const auto &entry : _parent->as< Directory >()->_items )
            if ( entry.inode() == this )
                return entry.name();
        __builtin_unreachable();
    }

    void parent( Node p ) { _parent = p; }
    Node parent() const { return _parent; }

    bool create( std::string_view name, Node inode, bool overwrite )
    {
        if ( name.size() > FILE_NAME_LIMIT )
            return error( ENAMETOOLONG ), false;
        if ( special_name( name ) )
            return error( EEXIST ), false;
        if ( _insertItem( DirectoryEntry( String( name ), std::move( inode ) ), overwrite ) )
        {
            inode->link();
            if ( inode->mode().is_dir() )
                link();
            return true;
        }
        return false;
    }

    Node find( std::string_view name )
    {
        if ( name == "." )
            return this;
        if ( name == ".." )
            return _parent ? _parent : this;

        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            return Node();
        return position->inode();
    }

    bool unlink( std::string_view name )
    {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            return error( ENOENT ), false;
        else
        {
            if ( position->inode()->mode().is_dir() )
                INode::unlink();
            position->inode()->unlink();
            _items.erase( position );
            return true;
        }
    }

    bool empty() { return _items.empty(); }

    template< typename T >
    T *find( const __dios::String &name ) {
        Node node = find( name );
        if ( !node )
            return nullptr;
        return node->as< T >();
    }

    bool read( char *buf, size_t offset, size_t &length ) override
    {
        if ( length != sizeof( struct dirent ) )
            return error( EINVAL ), false;

        int idx = offset / sizeof( struct dirent );

        auto do_read = [&]( const String &name, int inode )
        {
            struct dirent *dst = reinterpret_cast< struct dirent *>( buf );
            dst->d_ino = inode;
            char *x = std::copy( name.begin(), name.end(), dst->d_name );
            *x = '\0';
            return true;
        };

        if ( idx == 0 )
            return do_read( ".", ino() );
        if ( idx == 1 )
            return do_read( "..", _parent ? _parent->ino() : ino() );
        if ( idx - 2 == int( _items.size() ) )
            return length = 0, true;

        __dios_assert( idx - 2 < int( _items.size() ) );

        auto &item = _items[ idx - 2 ];
        return do_read( item.name(), item.inode()->ino() );
    }

private:

    bool _insertItem( DirectoryEntry &&entry, bool force )
    {
        auto position = _findItem( entry.name() );
        if ( position == _items.end() )
            _items.emplace_back( std::move( entry ) );
        else if ( position->name() != entry.name() )
            _items.insert( position, std::move( entry ) );
        else if ( force )
            *position = std::move( entry );
        else
            return error( EEXIST ), false;
        return true;
    }

    Items::iterator _findItem( std::string_view name )
    {
        return std::lower_bound(
            _items.begin(),
            _items.end(),
            name,
            []( const DirectoryEntry &entry, std::string_view name ) {
                return entry.name() < name;
            } );
    }

    Items _items;
    Node _parent;
};

}
