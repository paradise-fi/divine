#include "fs-manager.h"

namespace divine {
namespace fs {

FSManager::FSManager( bool ) :
    _root{ std::make_shared< INode >(
        Mode::DIR | Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER
    ) },
    _currentDirectory{ _root },
    _standardIO{ {
        std::make_shared< INode >( Mode::FILE | Mode::RUSER ),
        std::make_shared< INode >( Mode::FILE | Mode::RUSER )
    } },
    _openFD{
        std::make_shared< FileDescriptor >( _standardIO[ 0 ], flags::Open::Read ),// stdin
        std::make_shared< FileDescriptor >( _standardIO[ 1 ], flags::Open::Write ),// stdout
        std::make_shared< FileDescriptor >( _standardIO[ 1 ], flags::Open::Write )// stderr
    },
    _umask{ Mode::WGROUP | Mode::WOTHER }
{
    _root->assign( new Directory( _root ) );
    _standardIO[ 1 ]->assign( new WriteOnlyFile() );
}



void FSManager::createDirectoryAt( int dirfd, utils::String name, mode_t mode ) {
    if ( name.empty() )
        throw Error( ENOENT );

    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    mode &= ~umask() & ( Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER );
    mode |= Mode::DIR;
    if ( current->mode().hasGUID() )
        mode |= Mode::GUID;

    auto node = std::make_shared< INode >( mode );
    node->assign( new Directory( node, current ) );

    dir->create( std::move( name ), node );
}

void FSManager::createHardLinkAt( int newdirfd, utils::String name, int olddirfd, const utils::String &target, Flags< flags::At > fl ) {
    if ( name.empty() || target.empty() )
        throw Error( ENOENT );

    if ( fl.has( flags::At::Invalid ) )
        throw Error( EINVAL );

    Node current;
    {
        WeakNode savedDir = _currentDirectory;
        auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
        if ( path::isRelative( name ) &&  newdirfd != CURRENT_DIRECTORY )
            changeDirectory( newdirfd );
        std::tie( current, name )= _findDirectoryOfFile( name );
    }

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    Node targetNode;
    {
        WeakNode savedDir = _currentDirectory;
        auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
        if ( path::isRelative( target ) && olddirfd != CURRENT_DIRECTORY )
            changeDirectory( olddirfd );
        targetNode = findDirectoryItem( target, fl.has( flags::At::SymFollow ) );
    }
    if ( !targetNode )
        throw Error( ENOENT );

    if ( targetNode->mode().isDirectory() )
        throw Error( EPERM );

    dir->create( std::move( name ), targetNode );
}

void FSManager::createSymLinkAt( int dirfd, utils::String name, utils::String target ) {
    if ( name.empty() )
        throw Error( ENOENT );
    if ( target.size() > 1023 )
        throw Error( ENAMETOOLONG );

    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    mode_t mode = 0;
    mode |= Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER;
    mode |= Mode::LINK;

    auto node = std::make_shared< INode >( mode );
    node->assign( new Link( std::move( target ) ) );

    dir->create( std::move( name ), node );
}

void FSManager::createFifoAt( int dirfd, utils::String name, mode_t mode ) {
    if ( name.empty() )
        throw Error( ENOENT );

    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    mode &= ~umask() & ( Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER );
    mode |= Mode::FIFO;

    auto node = std::make_shared< INode >( mode );
    node->assign( new Pipe() );

    dir->create( std::move( name ), node );
}

ssize_t FSManager::readLinkAt( int dirfd, utils::String name, char *buf, size_t count ) {
    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node inode = findDirectoryItem( std::move( name ) );
    if ( !inode )
        throw Error( ENOENT );
    if ( !inode->mode().isLink() )
        throw Error( EINVAL );

    Link *sl = inode->data()->as< Link >();
    const utils::String &target = sl->target();
    auto realLength = std::min( target.size(), count );
    std::copy( target.c_str(), target.c_str() + realLength, buf );
    return realLength;
}

void FSManager::accessAt( int dirfd, utils::String name, Flags< flags::Access > mode, Flags< flags::At > fl ) {
    if ( name.empty() )
        throw Error( ENOENT );

    if ( mode.has( flags::Access::Invalid ) ||
        fl.has( flags::At::Invalid ) )
        throw Error( EINVAL );

    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node item = findDirectoryItem( name, !fl.has( flags::At::SymNofollow ) );
    if ( !item )
        throw Error( EACCES );

    if ( ( mode.has( flags::Access::Read ) && !item->mode().userRead() ) ||
         ( mode.has( flags::Access::Write ) && !item->mode().userWrite() ) ||
         ( mode.has( flags::Access::Execute ) && !item->mode().userExecute() ) )
        throw Error( EACCES );
}

int FSManager::openFileAt( int dirfd, utils::String name, Flags< flags::Open > fl, mode_t mode ) {
    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node file = findDirectoryItem( name, !fl.has( flags::Open::SymNofollow ) );

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
        _checkGrants( file, Mode::RUSER );
    if ( fl.has( flags::Open::Write ) ) {
        _checkGrants( file, Mode::WUSER );
        if ( file->mode().isDirectory() )
            throw Error( EISDIR );
        if ( fl.has( flags::Open::Truncate ) )
            if ( auto f = file->data()->as< File >() )
                f->clear();
    }

    if ( fl.has( flags::Open::NoAccess ) ) {
        fl.clear( flags::Open::Read );
        fl.clear( flags::Open::Write );
    }

    if ( file->mode().isFifo() )
        return _getFileDescriptor( std::make_shared< PipeDescriptor >( file, fl, true ) );
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

std::pair< int, int > FSManager::pipe() {
    mode_t mode = Mode::RWXUSER | Mode::FIFO;

    Node p = std::make_shared< INode >( mode, new Pipe( true, true ) );
    return {
        _getFileDescriptor( std::make_shared< PipeDescriptor >( p, flags::Open::Read ) ),
        _getFileDescriptor( std::make_shared< PipeDescriptor >( p, flags::Open::Write ) )
    };
}

void FSManager::removeFile( utils::String name ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    dir->remove( name );
}

void FSManager::removeDirectory( utils::String name ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );


    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    dir->removeDirectory( name );
}

void FSManager::removeAt( int dirfd, utils::String name, flags::At fl ) {
    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    switch( fl ) {
    case flags::At::NoFlags:
        removeFile( name );
        break;
    case flags::At::RemoveDir:
        removeDirectory( name );
        break;
    default:
        throw Error( EINVAL );
    }
}

void FSManager::renameAt( int newdirfd, utils::String newpath, int olddirfd, utils::String oldpath ) {
    Node oldNode;
    Directory *oldNodeDirectory;
    Node newNode;
    Directory *newNodeDirectory;

    utils::String oldName;
    utils::String newName;

    {
        WeakNode savedDir = _currentDirectory;
        auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
        if ( path::isRelative( oldpath ) && olddirfd != CURRENT_DIRECTORY )
            changeDirectory( olddirfd );

        std::tie( oldNode, oldName ) = _findDirectoryOfFile( oldpath );
        _checkGrants( oldNode, Mode::WUSER );
        oldNodeDirectory = oldNode->data()->as< Directory >();
    }
    oldNode = oldNodeDirectory->find( oldName );
    if ( !oldNode )
        throw Error( ENOENT );

    {
        WeakNode savedDir = _currentDirectory;
        auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
        if ( path::isRelative( newpath ) && newdirfd != CURRENT_DIRECTORY )
            changeDirectory( newdirfd );

        newNode = _findDirectoryItem( newpath, false, [&]( Node n ) {
                if ( n == oldNode )
                    throw Error( EINVAL );
            } );
    }

    if ( !newNode ) {
        std::tie( newNode, newName ) = _findDirectoryOfFile( newpath );
        _checkGrants( newNode, Mode::WUSER );

        newNodeDirectory = newNode->data()->as< Directory >();
        newNodeDirectory->create( std::move( newName ), oldNode );
    }
    else {
        if ( oldNode->mode().isDirectory() ) {
            if ( !newNode->mode().isDirectory() )
                throw Error( ENOTDIR );
            if ( newNode->size() > 2 )
                throw Error( ENOTEMPTY );
        }
        else if ( newNode->mode().isDirectory() )
            throw Error( EISDIR );

        newNodeDirectory->replaceEntry( newName, oldNode );
    }
    oldNodeDirectory->forceRemove( oldName );
}

off_t FSManager::lseek( int fd, off_t offset, Seek whence ) {
    auto f = getFile( fd );
    if ( f->inode()->mode().isFifo() )
        throw Error( ESPIPE );

    switch( whence ) {
    case Seek::Set:
        if ( offset < 0 )
            throw Error( EINVAL );
        f->offset( offset );
        break;
    case Seek::Current:
        if ( std::numeric_limits< size_t >::max() - f->offset() < offset )
            throw Error( EOVERFLOW );
        if ( offset < f->offset() )
            throw Error( EINVAL );
        f->offset( offset + f->offset() );
        break;
    case Seek::End:
        if ( std::numeric_limits< size_t >::max() - f->size() < offset )
            throw Error( EOVERFLOW );
        if ( f->size() < -offset )
            throw Error( EINVAL );
        f->offset( f->size() + offset );
        break;
    default:
        throw Error( EINVAL );
    }
    return f->offset();
}

void FSManager::truncate( Node inode, off_t length ) {
    if ( inode->mode().isDirectory() )
        throw Error( EISDIR );
    if ( !inode->mode().isFile() )
        throw Error( EINVAL );

    _checkGrants( inode, Mode::WUSER );

    RegularFile *f = inode->data()->as< RegularFile >();
    f->resize( length );
}

void FSManager::changeDirectory( utils::String pathname ) {
    Node item = findDirectoryItem( pathname );
    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _currentDirectory = item;
}

void FSManager::changeDirectory( int dirfd ) {
    Node item = getFile( dirfd )->inode();
    _checkGrants( item, Mode::XUSER );

    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _currentDirectory = item;
}

void FSManager::chmodAt( int dirfd, utils::String name, mode_t mode, Flags< flags::At > fl ) {
    if ( fl.has( flags::At::Invalid ) )
        throw Error( EINVAL );

    WeakNode savedDir = _currentDirectory;
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } );
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )
        changeDirectory( dirfd );

    Node inode = findDirectoryItem( std::move( name ), !fl.has( flags::At::SymNofollow ) );
    _chmod( inode, mode );
}

void FSManager::chmod( int fd, mode_t mode ) {
    _chmod( getFile( fd )->inode(), mode );
}

DirectoryDescriptor *FSManager::openDirectory( int fd ) {
    Node inode = getFile( fd )->inode();
    _checkGrants( inode, Mode::RUSER | Mode::XUSER );
    _openDD.emplace_back( , fd );
    return &_openDD.back();
}
DirectoryDescriptor *FSManager::getDirectory( void *descriptor ) {
    for ( auto i = _openDD.begin(); i != _openDD.end(); ++i ) {
        if ( &*i == descriptor ) {
            return &*i;
        }
    }
    throw Error( EBADF );
}
void FSManager::closeDirectory( void *descriptor ) {
    for ( auto i = _openDD.begin(); i != _openDD.end(); ++i ) {
        if ( &*i == descriptor ) {
            closeFile( i->fd() );
            _openDD.erase( i );
            return;
        }
    }
    throw Error( EBADF );
}

template< typename... Args >
void FSManager::_createFile( utils::String name, mode_t mode, Node *file, Args &&... args ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    mode &= ~umask() & ( Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER );
    mode |= Mode::FILE;

    auto node = std::make_shared< INode >( mode );
    node->assign( new RegularFile( std::forward< Args >( args )... ) );

    dir->create( std::move( name ), node );
    if ( file )
        *file = node;
}

Node FSManager::findDirectoryItem( utils::String name, bool followSymLinks ) {
    return _findDirectoryItem( std::move( name ), followSymLinks, []( Node ){} );
}
template< typename I >
Node FSManager::_findDirectoryItem( utils::String name, bool followSymLinks, I itemChecker ) {
    if ( name.size() > 1023 )
        throw Error( ENAMETOOLONG );
    name = path::normalize( name );
    Node current = _root;
    if ( path::isRelative( name ) )
        current = currentDirectory();

    Node item = current;
    utils::Queue< utils::String > q( path::splitPath< utils::Deque< utils::String > >( name ) );
    utils::Set< Link * > loopDetector;
    while ( !q.empty() ) {
        if ( !current->mode().isDirectory() )
            throw Error( ENOTDIR );

        _checkGrants( current, Mode::XUSER );

        Directory *dir = current->data()->as< Directory >();

        {
            auto d = utils::make_defer( [&]{ q.pop(); } );
            const auto &subFolder = q.front();
            if ( subFolder.empty() )
                continue;
            if ( subFolder.size() > 255 )
                throw Error( ENAMETOOLONG );
            item = dir->find( subFolder );
        }

        if ( !item ) {
            if ( q.empty() )
                return nullptr;
            throw Error( ENOENT );
        }

        itemChecker( item );

        if ( item->mode().isDirectory() )
            current = item;
        else if ( item->mode().isLink() && ( followSymLinks || !q.empty() ) ) {
            Link *sl = item->data()->as< Link >();

            if ( !loopDetector.insert( sl ).second )
                throw Error( ELOOP );

            utils::Queue< utils::String > _q( path::splitPath< utils::Deque< utils::String > >( sl->target() ) );
            while ( !q.empty() ) {
                _q.emplace( std::move( q.front() ) );
                q.pop();
            }
            q.swap( _q );
            if ( path::isAbsolute( sl->target() ) )
                current = _root;
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

std::pair< Node, utils::String > FSManager::_findDirectoryOfFile( utils::String name ) {
    name = path::normalize( name );
    utils::String pathname;
    std::tie( pathname, name ) = path::splitFileName( name );
    Node item = findDirectoryItem( pathname );
    _checkGrants( item, Mode::XUSER );

    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    return { item, name };
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
        createDirectoryAt( CURRENT_DIRECTORY, item.name, item.mode );
        break;
    case Type::SymLink:
        createSymLinkAt( CURRENT_DIRECTORY, item.name, item.content );
        break;
    default:
        break;
    }
}

void FSManager::_checkGrants( Node inode, mode_t grant ) const {
    if ( ( inode->mode() & grant ) != grant )
        throw Error( EACCES );
}

void FSManager::_chmod( Node inode, mode_t mode ) {
    inode->mode() =
        ( inode->mode() & ~Mode::CHMOD ) |
        ( mode & Mode::CHMOD );
}

} // namespace fs
} // namespace divine
