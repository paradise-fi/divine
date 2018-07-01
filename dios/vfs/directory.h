// -*- C++ -*- (c) 2015 Jiří Weiser

#include <memory>
#include <cerrno>

#include "inode.h"
#include "utils.h"
#include "constants.h"

#ifndef _FS_DIRECTORY_H_
#define _FS_DIRECTORY_H_

namespace __dios {
namespace fs {

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

    Directory( const __dios::String& name, WeakNode parent = WeakNode{} )
        : _items{}, _parent( parent ), _name( name )
    {}

    size_t size() const override { return _items.size() + 2; }
    const __dios::String& name() { return _name; }
    bool special_name( String name ) const { return name == "." || name == ".."; }

    void create( __dios::String name, Node inode ) {
        if ( name.size() > FILE_NAME_LIMIT )
            throw Error( ENAMETOOLONG );
        if ( special_name( name ) )
            throw Error( EEXIST );
        _insertItem( DirectoryEntry( std::move( name ), std::move( inode ) ) );
    }

    Node find( const __dios::String &name ) {
        if ( name == "." )
            return shared_from_this();
        if ( name == ".." )
            return _parent.lock();

        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            return Node();
        return position->inode();
    }

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
        if ( position->inode()->mode().isDirectory() )
            throw Error( EISDIR );
        _items.erase( position );
    }

    void removeDirectory( const __dios::String &name ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            throw Error( ENOENT );
        if ( !position->inode()->mode().isDirectory() )
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

    Items::iterator begin() {
        return _items.begin();
    }
    Items::iterator end() {
        return _items.end();
    }
    Items::const_iterator begin() const {
        return _items.begin();
    }
    Items::const_iterator end() const {
        return _items.end();
    }
private:

    void _insertItem( DirectoryEntry &&entry ) {
        auto position = _findItem( entry.name() );
        if ( position == _items.end() ) {
            _items.emplace_back( std::move( entry ) );
            return;
        }
        if ( position->name() != entry.name() ) {
            _items.insert( position, std::move( entry ) );
            return;
        }
        throw Error( EEXIST );
    }

    Items::iterator _findItem( const __dios::String &name ) {
        return std::lower_bound(
            _items.begin(),
            _items.end(),
            name,
            []( const DirectoryEntry &entry, const __dios::String &name ) {
                return entry.name() < name;
            } );
    }

    Items _items;
    WeakNode _parent;
     __dios::String _name;
};

} // namespace fs
} // namespace __dios

#endif
