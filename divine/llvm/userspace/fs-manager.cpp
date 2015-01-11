
#include "fs-manager.h"

namespace divine {
namespace fs {

void FSManager::createDirectory( utils::String name, unsigned mode ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Directory *current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    if ( !current )
        throw Error( ENOENT );

    if ( !current->grants().user.write() )
        throw Error( EACCES );

    if ( !current->create< Directory >( std::move( name ), mode ) )
        throw Error( EEXIST );
}

void FSManager::createHardLink( utils::String name, const utils::String &target ) {
    if ( name.empty() || target.empty() )
        throw Error( ENOENT );

    Directory *current;
    std::tie( current, name )= _findDirectoryOfFile( name );

    if ( !current )
        throw Error( ENOENT );

    if ( !current->grants().user.write() )
        throw Error( EACCES );

    Ptr targetPtr = findDirectoryItem( target );
    if ( !targetPtr )
        throw Error( ENOENT );

    if ( !targetPtr->isFile() )
        throw Error( EPERM );

    auto file = targetPtr->as< FileItem >();

    if ( !current->create< FileItem >( std::move( name ), Grants::NONE, file->object() ) )
        throw Error( EEXIST );
}

void FSManager::createSymLink( utils::String name, utils::String target ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Directory *current;
    std::tie( current, name ) = _findDirectoryOfFile( std::move( name ) );
    if ( !current )
        throw Error( ENOENT );

    if ( !current->grants().user.write() )
        throw Error( EACCES );

    if ( !current->create< SymLink >( std::move( name ), Grants::ALL, std::move( target ) ) )
        throw Error( EEXIST );
}

int FSManager::createFile( utils::String name, unsigned mode ) {
    Ptr file = nullptr;
    _createFile( std::move( name ), mode, &file );
    Flags< flags::Open > fl = flags::Open::Write | flags::Open::Create;// | flags::Open::Truncate
    return _getFileDescriptor( std::make_shared< FileDescriptor >( file, fl ) );
}

void FSManager::access( utils::String name, Flags< flags::Access > mode ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Ptr item = findDirectoryItem( name );
    if ( !item )
        throw Error( EACCES );

    if ( ( mode.has( flags::Access::Read ) && !item->grants().user.read() ) ||
         ( mode.has( flags::Access::Write ) && !item->grants().user.write() ) ||
         ( mode.has( flags::Access::Execute ) && !item->grants().user.execute() ) )
        throw Error( EACCES );
}

int FSManager::openFile( utils::String name, Flags< flags::Open > fl, unsigned mode ) {

    Ptr file = findDirectoryItem( name );
    if ( file && !file->isFile() )
        file = nullptr;

    if ( fl.has( flags::Open::Create ) ) {
        if ( file ) {
            if ( fl.has( flags::Open::Excl ) )
                throw Error( EEXIST );
        }
        else {
            _createFile( std::move( name ), mode, &file );
        }
    }
    else if ( !file )
        throw Error( ENOENT );

    if ( fl.has( flags::Open::Read ) )
        _checkGrants( file, Grants::READ );
    if ( fl.has( flags::Open::Write ) )
        _checkGrants( file, Grants::WRITE );

    return _getFileDescriptor( std::make_shared< FileDescriptor >( file, fl ) );
}

void FSManager::closeFile( int fd ) {
    getFile( fd ).reset();
}

int FSManager::duplicate( int oldfd ) {
    return _getFileDescriptor( getFile( oldfd ) );
}

int FSManager::duplicate2( int oldfd, int newfd ) {
    if ( oldfd == newfd )
        return newfd;
    auto f = getFile( oldfd );
    if ( newfd < 0 || newfd > 1024 )
        throw Error( EBADF );
    if ( newfd >= _openFD.size() )
        _openFD.resize( newfd + 1 );
    _openFD[ newfd ] = f;
    return newfd;
}

std::shared_ptr< FileDescriptor > &FSManager::getFile( int fd ) {
    if ( fd >= 0 && fd < _openFD.size() && _openFD[ fd ] )
        return _openFD[ fd ];
    throw Error( EBADF );
}

void FSManager::removeFile( utils::String name, Directory *current ) {
    if ( name.empty() )
        throw Error( ENOENT );

    if ( !current )
        std::tie( current, name ) = _findDirectoryOfFile( name );

    if ( !current )
        throw Error( ENOENT );

    _checkGrants( current, Grants::WRITE );

    if ( !current->find( name ) )
        throw Error( ENOENT );

    if ( !current->remove( name ) )
        throw Error( EISDIR );
}

void FSManager::removeDirectory( utils::String name, Directory *current ) {
    if ( name.empty() )
        throw Error( ENOENT );

    if ( !current )
        std::tie( current, name ) = _findDirectoryOfFile( name );

    if ( !current )
        throw Error( ENOENT );

    _checkGrants( current, Grants::WRITE );

    if ( !current->find( name ) )
        throw Error( ENOENT );

    if ( !current->removeDirectory( name ) )
        throw Error( ENOTDIR );

}

off_t FSManager::lseek( int fd, off_t offset, Seek whence ) {
    auto f = getFile( fd );
    if ( f->directoryItem()->isPipe() )
        throw Error( ESPIPE );

    switch( whence ) {
    case Seek::Set:
        if ( offset < 0 )
            throw Error( EINVAL );
        f->offset() = offset;
        break;
    case Seek::Current:
        if ( std::numeric_limits< size_t >::max() - f->offset() < offset )
            throw Error( EOVERFLOW );
        if ( offset < f->offset() )
            throw Error( EINVAL );
        f->offset() += offset;
        break;
    case Seek::End:
        if ( std::numeric_limits< size_t >::max() - f->size() < offset )
            throw Error( EOVERFLOW );
        if ( f->size() < -offset )
            throw Error( EINVAL );
        f->offset() = f->size() + offset;
        break;
    default:
        throw Error( EINVAL );
    }
    return f->offset();
}

template< typename... Args >
void FSManager::_createFile( utils::String name, unsigned mode, Ptr *file, Args &&... args ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Directory *current;
    std::tie( current, name ) = _findDirectoryOfFile( name );
    Ptr newFile;
    if ( !current )
        throw Error( ENOENT );

    _checkGrants( current, Grants::WRITE );

    auto content = std::make_shared< File >( std::forward< Args >( args )... );
    content->grants() = mode;
    bool result = (newFile = current->create< FileItem >( std::move( name ), Grants::NONE, content ) );
    if ( !result )
        throw Error( EEXIST );

    if ( file )
        *file = newFile;
}

Ptr FSManager::findDirectoryItem( const utils::String &name, bool followSymLinks ) {
    Directory *current = &_root;
    Ptr item = &_root;
    utils::Queue< utils::String > q( utils::splitPath< utils::Deque< utils::String > >( name ) );
    utils::Set< SymLink * > loopDetector;
    while ( !q.empty() ) {
        if ( !current )
            throw Error( ENOTDIR );

        _checkGrants( current, Grants::EXECUTE );

        {
            auto d = utils::make_defer( [&]() { q.pop(); } );
            const auto &subFolder = q.front();
            if ( subFolder.empty() )
                continue;
            item = current->find( subFolder );
        }
        if ( !item ) {
            if ( q.empty() )
                return nullptr;
            throw Error( ENOENT );
        }

        if ( item->isDirectory() )
            current = DirectoryItem::self( item )->as< Directory >();
        else if ( item->isSymLink() && ( followSymLinks || !q.empty() ) ) {
            SymLink *sl = item->as< SymLink >();

            if ( !loopDetector.insert( sl ).second )
                throw Error( ELOOP );

            utils::Queue< utils::String > _q( utils::splitPath< utils::Deque< utils::String > >( sl->target() ) );
            while ( !q.empty() ) {
                _q.emplace( std::move( q.front() ) );
                q.pop();
            }
            q.swap( _q );
            if ( q.front().empty() )
                current = current->parent()->as< Directory >();
            else
                current = &_root;
            continue;
        }
        else {
            if ( q.empty() )
                break;
            throw Error( ENOTDIR );
        }
    }
    return item;
}

std::pair< Directory *, utils::String > FSManager::_findDirectoryOfFile( utils::String name ) {
    utils::String path;
    std::tie( path, name ) = utils::splitFileName( name );
    return { findDirectoryItem( path )->as< Directory >(), name };
}

Directory *FSManager::_findDirectory( const utils::String &path ) {
    return findDirectoryItem( path )->as< Directory >();
}

int FSManager::_getFileDescriptor( std::shared_ptr< FileDescriptor > f ) {
    int i = 0;
    for ( auto &fd : _openFD ) {
        if ( !fd ) {
            fd = f;
            return i;
        }
        ++i;
    }
    if ( _openFD.size() >= 1024 )
        throw Error( ENFILE );

   _openFD.push_back( f );
    return i;
}

void FSManager::_insertSnapshotItem( const SnapshotFS &item ) {
    switch( item.type ) {
    case Type::File:
        _createFile( item.name, item.mode, nullptr, item.content, item.length );
        break;
    case Type::Directory:
        createDirectory( item.name, item.mode );
        break;
    case Type::SymLink:
        createSymLink( item.name, item.content );
        break;
    default:
        break;
    }
}

void FSManager::_checkGrants( const Grantsable *item, unsigned grant ) const {
    if ( ( item->grants().user & grant ) != grant )
        throw Error( EACCES );
}

} // namespace fs
} // namespace divine
