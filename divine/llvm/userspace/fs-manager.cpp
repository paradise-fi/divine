
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
    }
{
    _root->assign( new Directory( _root ) );
    _standardIO[ 1 ]->assign( new WriteOnlyFile() );
}



void FSManager::createDirectory( utils::String name, unsigned mode ) {
    if ( name.empty() )
        throw Error( ENOENT );

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

void FSManager::createHardLink( utils::String name, const utils::String &target ) {
    if ( name.empty() || target.empty() )
        throw Error( ENOENT );

    Node current;
    std::tie( current, name )= _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    Node targetNode = findDirectoryItem( target );
    if ( !targetNode )
        throw Error( ENOENT );

    if ( targetNode->mode().isDirectory() )
        throw Error( EPERM );

    dir->create( std::move( name ), targetNode );
}

void FSManager::createSymLink( utils::String name, utils::String target ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->data()->as< Directory >();

    unsigned mode = 0;
    mode |= Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER;
    mode |= Mode::LINK;

    auto node = std::make_shared< INode >( mode );
    node->assign( new Link( std::move( target ) ) );

    dir->create( std::move( name ), node );
}

int FSManager::createFile( utils::String name, unsigned mode ) {
    Node item;
    _createFile( std::move( name ), mode, &item );
    Flags< flags::Open > fl = flags::Open::Write | flags::Open::Create;// | flags::Open::Truncate
    return _getFileDescriptor( std::make_shared< FileDescriptor >( std::move( item ), fl ) );
}

void FSManager::access( utils::String name, Flags< flags::Access > mode ) {
    if ( name.empty() )
        throw Error( ENOENT );

    Node item = findDirectoryItem( name );
    if ( !item )
        throw Error( EACCES );

    if ( ( mode.has( flags::Access::Read ) && !item->mode().userRead() ) ||
         ( mode.has( flags::Access::Write ) && !item->mode().userWrite() ) ||
         ( mode.has( flags::Access::Execute ) && !item->mode().userExecute() ) )
        throw Error( EACCES );
}

int FSManager::openFile( utils::String name, Flags< flags::Open > fl, unsigned mode ) {

    Node file = findDirectoryItem( name );
    if ( file && !file->mode().isFile() )
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
        _checkGrants( file, Mode::RUSER );
    if ( fl.has( flags::Open::Write ) )
        _checkGrants( file, Mode::WUSER );

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

void FSManager::removeAt( int fd, utils::String name, flags::At fl ) {
    WeakNode savedDir = _currentDirectory;
    try {
        if ( utils::isRelative( name ) && fd != CURRENT_DIRECTORY )
            changeDirectory( fd );
        switch( fl ) {
        case flags::At::NoFlag:
            removeFile( name );
            break;
        case flags::At::RemoveDir:
            removeDirectory( name );
            break;
        default:
            throw Error( EINVAL );
        }
        _currentDirectory = savedDir;
    } catch ( Error & ) {
        _currentDirectory = savedDir;
        throw;
    }
}

off_t FSManager::lseek( int fd, off_t offset, Seek whence ) {
    auto f = getFile( fd );
    if ( f->inode()->mode().isFifo() )
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

void FSManager::changeDirectory( utils::String path ) {
    Node item = findDirectoryItem( path );
    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _currentDirectory = item;
}

void FSManager::changeDirectory( int fd ) {
    Node item = getFile( fd )->inode();
    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _currentDirectory = item;
}

template< typename... Args >
void FSManager::_createFile( utils::String name, unsigned mode, Node *file, Args &&... args ) {
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
    name = utils::normalize( name );
    Node current = _root;
    if ( utils::isRelative( name ) )
        current = currentDirectory();

    Node item = current;
    utils::Queue< utils::String > q( utils::splitPath< utils::Deque< utils::String > >( name ) );
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
            item = dir->find( subFolder );
        }

        if ( !item ) {
            if ( q.empty() )
                return nullptr;
            throw Error( ENOENT );
        }

        if ( item->mode().isDirectory() )
            current = item;
        else if ( item->mode().isLink() && ( followSymLinks || !q.empty() ) ) {
            Link *sl = item->data()->as< Link >();

            if ( !loopDetector.insert( sl ).second )
                throw Error( ELOOP );

            utils::Queue< utils::String > _q( utils::splitPath< utils::Deque< utils::String > >( sl->target() ) );
            while ( !q.empty() ) {
                _q.emplace( std::move( q.front() ) );
                q.pop();
            }
            q.swap( _q );
            if ( utils::isAbsolute( sl->target() ) )
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
    name = utils::normalize( name );
    utils::String path;
    std::tie( path, name ) = utils::splitFileName( name );
    Node item = findDirectoryItem( path );
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
        createDirectory( item.name, item.mode );
        break;
    case Type::SymLink:
        createSymLink( item.name, item.content );
        break;
    default:
        break;
    }
}

void FSManager::_checkGrants( Node inode, unsigned grant ) const {
    if ( ( inode->mode() & grant ) != grant )
        throw Error( EACCES );
}

} // namespace fs
} // namespace divine
