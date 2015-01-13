#include <memory>
#include <cerrno>

#include "fs-inode.h"
#include "fs-utils.h"

#ifndef _FS_DIRECTORY_H_
#define _FS_DIRECTORY_H_

namespace divine {
namespace fs {

struct DirectoryEntry {

    DirectoryEntry( utils::String name, Node inode ) :
        _name( std::move( name ) ),
        _inode( std::move( inode ) ),
        _weak( false )
    {}

    DirectoryEntry( utils::String name, WeakNode inode ) :
        _name( std::move( name ) ),
        _inode( std::move( inode ) ),
        _weak( true )
    {}

    DirectoryEntry( const DirectoryEntry &other ) :
        _name( other.name() ),
        _weak( other._weak )
    {
        if ( _weak )
            new (&_inode.weak) WeakNode( other._inode.weak );
        else
            new (&_inode.strong) Node( other._inode.strong );
    }

    DirectoryEntry( DirectoryEntry &&other ) :
        _name( std::move( other._name ) ),
        _weak( other._weak )
    {
        if ( _weak )
            new (&_inode.weak) WeakNode( std::move( other._inode.weak ) );
        else
            new (&_inode.strong) Node( std::move( other._inode.strong ) );
    }

    ~DirectoryEntry() {
        if ( _weak )
            _inode.weak.~WeakNode();
        else
            _inode.strong.~Node();
    }

    DirectoryEntry &operator=( DirectoryEntry other ) {
        swap( other );
        return *this;
    }

    utils::String &name() {
        return _name;
    }
    const utils::String &name() const {
        return _name;
    }

    Node inode() {
        return _weak ? _inode.weak.lock() : _inode.strong;
    }
    const Node inode() const {
        return _weak ? _inode.weak.lock() : _inode.strong;
    }

    void swap( DirectoryEntry &other ) {
        using std::swap;
        swap( _name, other._name );

        if ( _weak && other._weak )
            _inode.weak.swap( other._inode.weak );
        else if ( !_weak && !other._weak )
            _inode.strong.swap( other._inode.strong );
        else if ( _weak ) {
            WeakNode tmp( std::move( _inode.weak ) );
            new (&_inode.strong) Node( std::move( other._inode.strong ) );
            new (&other._inode.weak) WeakNode( std::move( tmp ) );
        }
        else {
            WeakNode tmp( std::move( other._inode.weak ) );
            new (&other._inode.strong) Node( std::move( _inode.strong ) );
            new (&_inode.weak) WeakNode( std::move( tmp ) );
        }
        swap( _weak, other._weak );
    }

private:
    utils::String _name;
    union _U {
        Node strong;
        WeakNode weak;
        _U() {}
        _U( Node &&node ) :
            strong( std::move( node ) )
        {}
        _U( WeakNode &&node ) :
            weak( std::move( node ) )
        {}
        _U( const _U & ) {}
        _U( _U && ) {}
        ~_U() {}
    } _inode;
    bool _weak;

};

struct Directory : DataItem {
    using Items = utils::Vector< DirectoryEntry >;

    Directory( WeakNode self, WeakNode parent = WeakNode{} ) :
        _items{
            DirectoryEntry{ ".", self },
            DirectoryEntry{ "..", !parent.expired() ? parent : self }
        }
    {}

    size_t size() const override {
        return _items.size();
    }

    void create( utils::String name, Node inode ) {
        _insertItem( DirectoryEntry( std::move( name ), std::move( inode ) ) );
    }

    Node find( const utils::String &name ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            return Node();
        return position->inode();
    }

    template< typename T >
    T *find( const utils::String &name ) {
        Node node = find( name );
        if ( !node )
            return nullptr;
        return node->data()->as< T >();
    }

    void remove( const utils::String &name ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            throw Error( ENOENT );
        if ( position->inode()->mode().isDirectory() )
            throw Error( EISDIR );
        _items.erase( position );
    }

    void removeDirectory( const utils::String &name ) {
        auto position = _findItem( name );
        if ( position == _items.end() || name != position->name() )
            throw Error( ENOENT );
        if ( !position->inode()->mode().isDirectory() )
            throw Error( ENOTDIR );

        if ( position->inode()->size() != 2 )
            throw Error( ENOTEMPTY );

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

    Items::iterator _findItem( const utils::String &name ) {
        return std::lower_bound(
            _items.begin(),
            _items.end(),
            name,
            []( const DirectoryEntry &entry, const utils::String &name ) {
                return entry.name() < name;
            } );
    }

    Items _items;
};

} // namespace fs
} // namespace divine

#endif
