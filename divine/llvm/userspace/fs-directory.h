#include <memory>
#include <cerrno>

#include "fs-constants.h"
#include "fs-file.h"
#include "fs-utils.h"

#ifndef _FS_DIRECTORY_H_
#define _FS_DIRECTORY_H_

namespace divine {
namespace fs {

struct Typeable {
    virtual ~Typeable(){}
    virtual bool isFile() const { return false; }
    virtual bool isPipe() const { return false; }
    virtual bool isSymLink() const { return false; }
    virtual bool isDirectory() const { return false; }

    template< typename T >
    T *as() {
        return dynamic_cast< T * >( this );
    }
    template< typename T >
    const T *as() const {
        return dynamic_cast< const T * >( this );
    }
};

struct DirectoryItem : Typeable, Grantsable {

    DirectoryItem( DirectoryItem *parent, utils::String name, unsigned mode = Grants::ALL ) :
        _parent( parent ),
        _name( std::move( name ) ),
        _ino( getIno() ),
        _grants( mode )
    {}

    DirectoryItem( const DirectoryItem & ) = delete;
    DirectoryItem &operator=( const DirectoryItem & ) = delete;
    DirectoryItem( DirectoryItem && ) = default;
    DirectoryItem &operator=( DirectoryItem && ) = default;

    const utils::String &name() const {
        return _name;
    }
    utils::String &name() {
        return _name;
    }

    DirectoryItem *parent() {
        return _parent;
    }
    const DirectoryItem *parent() const {
        return _parent;
    }

    static DirectoryItem *self( DirectoryItem *item ) {
        return item->selfVirtual();
    }

    int ino() const {
        return _ino;
    }

    Grants &grants() override {
        return _grants;
    }
    Grants grants() const override {
        return _grants;
    }

    virtual size_t size() const {
        return 0;
    }

protected:

    virtual DirectoryItem *selfVirtual() {
        return this;
    }
private:
    DirectoryItem *_parent;
    utils::String _name;
    const int _ino;
    Grants _grants;

    static int getIno() {
        static int lastIno = 0;
        return ++lastIno;
    }

};

using Handle = std::unique_ptr< DirectoryItem >;
using Ptr = DirectoryItem *;
using ConstPtr = const DirectoryItem *;


struct FileItem : DirectoryItem {

    FileItem( DirectoryItem *parent, utils::String name, unsigned mode, std::shared_ptr< FSItem > item ) :
        DirectoryItem( parent, std::move( name ) ),
        _item( item )
    {}

    bool isFile() const override {
        return true;
    }
    bool isPipe() const override {
        return !_item->isRegularFile();
    }

    size_t size() const override {
        return _item->size();
    }

    Grants &grants() override {
        return _item->grants();
    }
    Grants grants() const override {
        return _item->grants();
    }

    FSItem &get() {
        return *_item;
    }
    const FSItem &get() const {
        return *_item;
    }

    template< typename T >
    T *get() {
        return _item.get() ? dynamic_cast< T * >( _item.get() ) : nullptr;
    }
    template< typename T >
    const T *get() const {
        return _item.get() ? dynamic_cast< const T * >( _item.get() ) : nullptr;
    }

    std::shared_ptr< FSItem > object() {
        return _item;
    }

    const std::shared_ptr< FSItem > &object() const {
        return _item;
    }
private:
    std::shared_ptr< FSItem > _item;
};

struct SymLink : DirectoryItem {
    
    SymLink( DirectoryItem *parent, utils::String name, unsigned mode, utils::String target ) :
        DirectoryItem( parent, std::move( name ), mode ),
        _target( std::move( target ) )
    {}

    bool isSymLink() const override {
        return true;
    }

    size_t size() const override {
        return _target.size();
    }

    utils::String &target() {
        return _target;
    }
    const utils::String &target() const {
        return _target;
    }
private:
    utils::String _target;
};

struct Directory : DirectoryItem {
    using Items = utils::Vector< Handle >;

    struct DirectoryAlias : DirectoryItem {

        DirectoryAlias( Directory *aliasFor, utils::String name ) :
            DirectoryItem( aliasFor->parent(), std::move( name ) ),
            _aliasFor( *aliasFor )
        {
        }

        bool isDirectory() const override { return true; }

        template< typename T, typename... Args >
        Ptr create( utils::String name, unsigned mode, Args &&... args ) {
            return _aliasFor.create< T >( std::move( name ), mode, std::forward< Args >( args )... );
        }

        Ptr find( utils::String name ) {
            return _aliasFor.find( name );
        }
        template< typename T >
        T *find( utils::String name ) {
            return _aliasFor.find< T >( std::move( name ) );
        }

        bool remove( utils::String name ) {
            return _aliasFor.remove( std::move( name ) );
        }

        bool removeDirectory( utils::String name ) {
            return _aliasFor.removeDirectory( std::move( name ) );
        }

        size_t size() const {
            return _aliasFor.size();
        }

        Items::iterator begin() {
            return _aliasFor.begin();
        }
        Items::iterator end() {
            return _aliasFor.end();
        }
        Items::const_iterator begin() const {
            return _aliasFor.begin();
        }
        Items::const_iterator end() const {
            return _aliasFor.end();
        }
    protected:
        DirectoryItem *selfVirtual() override {
            return &_aliasFor;
        }
    private:
        Directory &_aliasFor;
    };

    Directory() :
        Directory( this, "", Grants::ALL )
    {}

    Directory( Directory *parent, utils::String name, unsigned mode ) :
        DirectoryItem( parent, std::move( name ), mode )
    {
        _items.emplace_back( Handle( new DirectoryAlias( this, "." ) ) );
        _items.emplace_back( Handle( new DirectoryAlias( parent, ".." ) ) );
    }

    bool isDirectory() const override { return true; }

    template< typename T, typename... Args >
    Ptr create( utils::String name, unsigned mode, Args &&... args ) {
        return insertDirectoryItem( name, Handle( new T( this, name, mode, std::forward< Args >( args )... ) ) );
    }

    Ptr find( utils::String name ) {
        auto position = findItem( name );
        if ( position == end() || name != (*position)->name() )
            return nullptr;
        return position->get();
    }

    template< typename T >
    T *find( utils::String name ) {
        Ptr item = find( std::move( name ) );
        if ( !item )
            return nullptr;
        if ( item->isFile() )
            return item->as< FileItem >()->get< T >();
        return self( item )->as< T >();
    }

    bool remove( const utils::String &name ) {
        auto position = findItem( name );
        if ( position == end() || name != (*position)->name() || (*position)->isDirectory() )
            return false;
        _items.erase( position );
        return true;
    }

    bool removeDirectory( const utils::String &name ) {
        if ( name == "." || name == ".." )
            return false;
        auto position = findItem( name );
        if ( position == end() || name != (*position)->name() || !(*position)->isDirectory() )
            return false;
        Directory *dir = (*position)->as< Directory >();
        if ( !dir || dir->size() != 2 ) // not empty
            return false;
        _items.erase( position );
        return true;
    }

    size_t size() const {
        return _items.size();
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

    Ptr insertDirectoryItem( const utils::String &name, Handle &&handle ) {
        Ptr result = handle.get();
        auto position = findItem( name );
        if ( position == end() ) {
            _items.push_back( std::move( handle ) );
            return result;
        }
        if ( (*position)->name() != name) {
            _items.insert( position, std::move( handle ) );
            return result;
        }
        errno = EEXIST;
        return nullptr;
    }

    Items::iterator findItem( const utils::String &name ) {
        return std::lower_bound(
            _items.begin(),
            _items.end(),
            name,
            []( const Handle &handle, const utils::String &name ) {
                return handle->name() < name;
            } );
    }

    Items _items;
};

} // namespace fs
} // namespace divine

#endif
