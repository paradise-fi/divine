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

struct DirectoryItemLabel {
    DirectoryItemLabel( const DirectoryEntry &entry ) :
        _name( entry.name() ),
        _ino( entry.inode()->ino() )
    {}

    DirectoryItemLabel( const DirectoryItemLabel & ) = default;
    DirectoryItemLabel( DirectoryItemLabel && ) = default;
    DirectoryItemLabel &operator=( DirectoryItemLabel other ) {
        _name.swap( other._name );
        _ino = other._ino;
        return *this;
    }

    const __dios::String &name() const {
        return _name;
    }
    unsigned ino() const {
        return _ino;
    }

private:
    __dios::String _name;
    unsigned _ino;
};

struct Directory : INode, std::enable_shared_from_this< Directory >
{
    using Items = __dios::Vector< DirectoryEntry >;

    Directory( WeakNode parent = WeakNode{} )
        : _items{}, _parent( parent )
    {}

    size_t size() const override { return _items.size() + 2; }
    bool special_name( std::string_view name ) const { return name == "." || name == ".."; }

    std::string_view name()
    {
        auto parent = _parent.lock();
        if ( parent.get() == this )
            return "/";
        for ( const auto &entry : parent->as< Directory >()->_items )
            if ( entry.inode().get() == this )
                return entry.name();
        __builtin_unreachable();
    }

    void parent( WeakNode p ) { _parent = p; }
    Node parent() const { return _parent.lock(); }

    bool create( std::string_view name, Node inode, bool overwrite )
    {
        if ( name.size() > FILE_NAME_LIMIT )
            return error( ENAMETOOLONG ), false;
        if ( special_name( name ) )
            return error( EEXIST ), false;
        inode->link();
        return _insertItem( DirectoryEntry( String( name ), std::move( inode ) ), overwrite );
    }

    Node find( std::string_view name )
    {
        if ( name == "." )
            return shared_from_this();
        if ( name == ".." )
            return _parent.expired() ? shared_from_this() : _parent.lock();

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
            position->inode()->unlink();
            _items.erase( position );
            return true;
        }
    }

    bool empty() { return _items.empty(); }

    void replaceEntry( const __dios::String &name, Node node ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            throw Error( ENOENT );
        *position = DirectoryEntry( name, node );
    }

    template< typename T >
    T *find( const __dios::String &name ) {
        Node node = find( name );
        if ( !node )
            return nullptr;
        return node->as< T >();
    }

    void remove( const __dios::String &name ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            throw Error( ENOENT );
        if ( position->inode()->mode().is_dir() )
            throw Error( EISDIR );
        _items.erase( position );
    }

    void removeDirectory( const __dios::String &name ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            throw Error( ENOENT );
        if ( !position->inode()->mode().is_dir() )
            throw Error( ENOTDIR );

        if ( position->inode()->size() != 2 )
            throw Error( ENOTEMPTY );

        _items.erase( position );
    }

    void forceRemove( const __dios::String &name ) {
        auto position = _findItem( name );
        if ( position != _items.end() && name == position->name() )
            _items.erase( position );
    }

    bool read( char *buf, size_t offset, size_t &length ) override
    {
        if ( length != sizeof( struct dirent ) )
            throw Error( EINVAL );

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
            return do_read( "..", _parent.expired() ? ino() : _parent.lock()->ino() );
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
    WeakNode _parent;
};

}