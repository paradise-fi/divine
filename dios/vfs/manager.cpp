// -*- C++ -*- (c) 2015 Jiří Weiser

#include "manager.h"
#include <dios/sys/memory.hpp>

#define REMEMBER_DIRECTORY( dirfd, name )                               \
    WeakNode savedDir = _currentDirectory;                               \
    auto d = utils::make_defer( [&]{ _currentDirectory = savedDir; } ); \
    if ( path::isRelative( name ) && dirfd != CURRENT_DIRECTORY )       \
        changeDirectory( dirfd );                                       \
    else                                                                \
        d.pass();

namespace __dios {
namespace fs {

Manager::Manager( bool ) :
    _root( new( __dios::nofail ) Directory( "/" ) ),
    _error( 0 ),
    _currentDirectory( _root ),
    _standardIO{ { make_shared< StandardInput >(), nullptr, nullptr } }
{
    _root->mode( Mode::DIR | Mode::GRANTS );
}

void Manager::setOutputFile( FileTrace trace )
{
    switch (trace) {
        case FileTrace::UNBUFFERED:
            _standardIO[ 1 ].reset( new( __dios::nofail ) VmTraceFile() );
            break;
        case FileTrace::TRACE:
             _standardIO[ 1 ].reset( new( __dios::nofail ) VmBuffTraceFile() );
             break;
        default :
            break;
    }
}

void Manager::setErrFile(FileTrace trace) {
    switch (trace) {
        case FileTrace::UNBUFFERED:
            _standardIO[ 2 ].reset( new( __dios::nofail ) VmTraceFile() );
            break;
        case FileTrace::TRACE:
             _standardIO[ 2 ].reset( new( __dios::nofail ) VmBuffTraceFile() );
             break;
        default :
            break;
    }
}


template< typename... Args >
Node Manager::createNodeAt( int dirfd, __dios::String name, mode_t mode, Args &&... args ) {
    if ( name.empty() )
        throw Error( ENOENT );

    REMEMBER_DIRECTORY( dirfd, name );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->as< Directory >();

    mode &= ~umask() & ( Mode::TMASK | Mode::GRANTS );
    if ( Mode( mode ).isDirectory() )
        mode |= Mode::GUID;

    Node node;

    switch( mode & Mode::TMASK )
    {
        case Mode::SOCKET:
            node.reset( utils::constructIfPossible< SocketDatagram >( std::forward< Args >( args )... ) );
            break;
        case Mode::LINK:
            node.reset( utils::constructIfPossible< SymLink >( std::forward< Args >( args )... ) );
            break;
        case Mode::FILE:
            node.reset( utils::constructIfPossible< RegularFile >( std::forward< Args >( args )... ) );
            break;
        case Mode::DIR:
            node.reset( utils::constructIfPossible< Directory >( name, current ) );
            break;
        case Mode::FIFO:
            node.reset( utils::constructIfPossible< Pipe >( std::forward< Args >( args )... ) );
            break;
        case Mode::BLOCKD:
        case Mode::CHARD:
            throw Error( EPERM );
        default:
            throw Error( EINVAL );
    }

    if ( !node )
        throw Error( EINVAL );

    node->mode( mode );
    dir->create( std::move( name ), node );

    return node;
}

void Manager::createHardLinkAt( int newdirfd, __dios::String name, int olddirfd, const __dios::String &target, bool follow )
{
    if ( name.empty() || target.empty() )
        throw Error( ENOENT );

    Node current;
    {
        REMEMBER_DIRECTORY( newdirfd, name );
        std::tie( current, name )= _findDirectoryOfFile( name );
    }

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->as< Directory >();

    Node targetNode;
    {
        REMEMBER_DIRECTORY( olddirfd, target );
        targetNode = findDirectoryItem( target, follow );
    }

    if ( !targetNode )
        throw Error( ENOENT );

    if ( targetNode->mode().isDirectory() )
        throw Error( EPERM );

    dir->create( std::move( name ), targetNode );
}

void Manager::createSymLinkAt( int dirfd, __dios::String name, __dios::String target ) {
    if ( name.empty() )
        throw Error( ENOENT );
    if ( target.size() > PATH_LIMIT )
        throw Error( ENAMETOOLONG );

    mode_t mode = 0;
    mode |= Mode::RWXUSER | Mode::RWXGROUP | Mode::RWXOTHER;
    mode |= Mode::LINK;

    createNodeAt( dirfd, std::move( name ), mode, std::move( target ) );
}

ssize_t Manager::readLinkAt( int dirfd, __dios::String name, char *buf, size_t count ) {
    REMEMBER_DIRECTORY( dirfd, name );

    Node inode = findDirectoryItem( std::move( name ), false );
    if ( !inode )
        throw Error( ENOENT );
    if ( !inode->mode().isLink() )
        throw Error( EINVAL );

    SymLink *sl = inode->as< SymLink >();
    const __dios::String &target = sl->target();
    auto realLength = std::min( target.size(), count );
    std::copy( target.c_str(), target.c_str() + realLength, buf );
    return realLength;
}

void Manager::accessAt( int dirfd, __dios::String name, int mode, bool follow )
{
    if ( name.empty() )
        throw Error( ENOENT );

    REMEMBER_DIRECTORY( dirfd, name );

    Node item = findDirectoryItem( name, follow );
    if ( !item )
        throw Error( ENOENT );

    if ( ( ( mode & R_OK ) && !item->mode().userRead() ) ||
         ( ( mode & W_OK ) && !item->mode().userWrite() ) ||
         ( ( mode & X_OK ) && !item->mode().userExecute() ) )
        throw Error( EACCES );
}

int Manager::openFileAt( int dirfd, __dios::String name, Flags< flags::Open > fl, mode_t mode )
{
    REMEMBER_DIRECTORY( dirfd, name );

    Node file = findDirectoryItem( name, !fl.has( flags::Open::SymNofollow ) );

    if ( fl.has( flags::Open::Create ) ) {
        if ( file ) {
            if ( fl.has( flags::Open::Excl ) )
                throw Error( EEXIST );
        }
        else {
            file = createNodeAt( CURRENT_DIRECTORY, std::move( name ), mode | Mode::FILE );
        }
    } else if ( !file ) {
        throw Error( ENOENT );
    }

    if ( fl.has( flags::Open::Read ) )
        _checkGrants( file, Mode::RUSER );
    if ( fl.has( flags::Open::Write ) ) {
        _checkGrants( file, Mode::WUSER );
        if ( file->mode().isDirectory() )
            throw Error( EISDIR );
        if ( fl.has( flags::Open::Truncate ) )
            file->clear();
    }

    if ( fl.has( flags::Open::NoAccess ) ) {
        fl.clear( flags::Open::Read );
        fl.clear( flags::Open::Write );
    }

    if ( fl.has( flags::Open::Directory ) ) {
        if (!file->mode().isDirectory())
            throw Error( ENOTDIR );
        _checkGrants( file, Mode::RUSER | Mode::XUSER );
    }

    return _getFileDescriptor( file, fl | flags::Open::FifoWait );
}

void Manager::closeFile( int fd )
{
    __dios_assert( fd >= 0 && fd < int( _proc->_openFD.size() ) );
    _proc->_openFD[ fd ].reset();
}

int Manager::duplicate( int oldfd, int lowEdge ) {
    return _getFileDescriptor( getFile( oldfd ), lowEdge );
}

int Manager::duplicate2( int oldfd, int newfd ) {
    if ( oldfd == newfd )
        return newfd;
    auto f = getFile( oldfd );
    if ( newfd < 0 || newfd > FILE_DESCRIPTOR_LIMIT )
        throw Error( EBADF );
    if ( newfd >= int( _proc->_openFD.size() ) )
        _proc->_openFD.resize( newfd + 1 );
    _proc->_openFD[ newfd ] = f;
    return newfd;
}

std::shared_ptr< FileDescriptor > Manager::getFile( int fd )
{
    if ( fd >= 0 && fd < int( _proc->_openFD.size() ) && _proc->_openFD[ fd ] )
        return _proc->_openFD[ fd ];
    return nullptr;
}

std::shared_ptr< Socket > Manager::getSocket( int sockfd ) {
    auto f = std::dynamic_pointer_cast< Socket >( getFile( sockfd )->inode() );
    if ( !f )
        throw Error( ENOTSOCK );
    return f;
}

std::pair< int, int > Manager::pipe()
{
    mode_t mode = Mode::RWXUSER | Mode::FIFO;

    Node node( new ( nofail ) Pipe() );
    node->mode( mode );
    auto fd1 = _getFileDescriptor( node, flags::Open::Read );
    auto fd2 =  _getFileDescriptor( node, flags::Open::Write );
    return { fd1, fd2 };
}

void Manager::removeFile( int dirfd, __dios::String name ) {
    if ( name.empty() )
        throw Error( ENOENT );

    REMEMBER_DIRECTORY( dirfd, name );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );

    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->as< Directory >();

    dir->remove( name );
}

void Manager::removeDirectory( int dirfd, __dios::String name ) {
    if ( name.empty() )
        throw Error( ENOENT );

    REMEMBER_DIRECTORY( dirfd, name );

    Node current;
    std::tie( current, name ) = _findDirectoryOfFile( name );


    _checkGrants( current, Mode::WUSER );
    Directory *dir = current->as< Directory >();

    dir->removeDirectory( name );
}

void Manager::renameAt( int newdirfd, __dios::String newpath, int olddirfd, __dios::String oldpath ) {
    Node oldNode;
    Directory *oldNodeDirectory;
    Node newNode;
    Directory *newNodeDirectory;

    __dios::String oldName;
    __dios::String newName;

    {
        REMEMBER_DIRECTORY( olddirfd, oldpath );

        std::tie( oldNode, oldName ) = _findDirectoryOfFile( oldpath );
        _checkGrants( oldNode, Mode::WUSER );
        oldNodeDirectory = oldNode->as< Directory >();
    }
    oldNode = oldNodeDirectory->find( oldName );
    if ( !oldNode )
        throw Error( ENOENT );

    {
        REMEMBER_DIRECTORY( newdirfd, newpath );

        newNode = _findDirectoryItem( newpath, false, [&]( Node n ) {
                if ( n == oldNode )
                    throw Error( EINVAL );
            } );
    }

    if ( !newNode ) {
        std::tie( newNode, newName ) = _findDirectoryOfFile( newpath );
        _checkGrants( newNode, Mode::WUSER );

        newNodeDirectory = newNode->as< Directory >();
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

        newNodeDirectory = newNode->as< Directory >();
        newNodeDirectory->replaceEntry( newName, oldNode );
    }
    oldNodeDirectory->forceRemove( oldName );
}

off_t Manager::lseek( int fd, off_t offset, Seek whence ) {
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
        if ( offset > 0 && std::numeric_limits< size_t >::max() - f->offset() < size_t( offset ) )
            throw Error( EOVERFLOW );
        if ( int( f->offset() ) < -offset )
            throw Error( EINVAL );
        f->offset( offset + f->offset() );
        break;
    case Seek::End:
        if ( offset > 0 && std::numeric_limits< size_t >::max() - f->size() < size_t( offset ) )
            throw Error( EOVERFLOW );
        if ( offset < 0 && int( f->size() ) < -offset )
            throw Error( EINVAL );
        f->offset( f->size() + offset );
        break;
    default:
        throw Error( EINVAL );
    }
    return f->offset();
}

void Manager::truncate( Node inode, off_t length ) {
    if ( !inode )
        throw Error( ENOENT );
    if ( length < 0 )
        throw Error( EINVAL );
    if ( inode->mode().isDirectory() )
        throw Error( EISDIR );
    if ( !inode->mode().isFile() )
        throw Error( EINVAL );

    _checkGrants( inode, Mode::WUSER );

    RegularFile *f = inode->as< RegularFile >();
    f->resize( length );
}

void Manager::getCurrentWorkingDir( char *buff, size_t size ) {
    if ( buff == nullptr )
        throw Error( EFAULT );
    if ( size == 0 )
        throw Error( EINVAL );
    __dios::String path = "";
    __dios::String name;
    Node originalDirectory = currentDirectory( );

    Node current = originalDirectory;
    while( current != _root ) {
        Directory *dir = current->as< Directory >( );
        if ( !dir )
            break;
        name = "/" + dir->name( );
        path = name + path;
        current = dir->find( ".." );
    }
    if ( path.empty( ) )
        path = "/";

    if ( path.size( ) >= size )
        throw Error( ERANGE );
    char *end =std::copy( path.c_str(), path.c_str() + path.length(), buff );
    *end = '\0';
}

void Manager::changeDirectory( __dios::String pathname ) {
    Node item = findDirectoryItem( pathname );
    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _checkGrants( item, Mode::XUSER );

    _currentDirectory = item;
}

void Manager::changeDirectory( int dirfd ) {
    auto fd = getFile( dirfd );
    if ( !fd )
        throw Error( EBADF );
    Node item = fd->inode();
    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _checkGrants( item, Mode::XUSER );

    _currentDirectory = item;
}

void Manager::chmodAt( int dirfd, __dios::String name, mode_t mode, bool follow )
{
    REMEMBER_DIRECTORY( dirfd, name );

    Node inode = findDirectoryItem( std::move( name ), follow );
    _chmod( inode, mode );
}

void Manager::chmod( int fd, mode_t mode ) {
    _chmod( getFile( fd )->inode(), mode );
}

int Manager::socket( SocketType type, Flags< flags::Open > fl )
{
    Socket *s = nullptr;
    switch ( type ) {
    case SocketType::Stream:
        s = new( __dios::nofail ) SocketStream;
        break;
    case SocketType::Datagram:
        s = new( __dios::nofail ) SocketDatagram;
        break;
    default:
        throw Error( EPROTONOSUPPORT );
    }

    Node socket( s );
    socket->mode( Mode::GRANTS | Mode::SOCKET );

    return _getFileDescriptor( socket, fl );
}

std::pair< int, int > Manager::socketpair( SocketType type, Flags< flags::Open > fl ) {
    if ( type != SocketType::Stream )
        throw Error( EOPNOTSUPP );

    SocketStream *cl = new( __dios::nofail ) SocketStream;
    Node client( cl );
    client->mode( Mode::GRANTS | Mode::SOCKET );

    Node server( new( __dios::nofail ) SocketStream );
    server->mode( Mode::GRANTS | Mode::SOCKET );

    cl->connected( client, server );

    return {
        _getFileDescriptor( server, fl ),
        _getFileDescriptor( client, fl )
    };
}

void Manager::bind( int sockfd, Socket::Address address ) {
    auto sd = getSocket( sockfd );

    Node current;
    __dios::String name = address.value();
    std::tie( current, name ) = _findDirectoryOfFile( name );

    Directory *dir = current->as< Directory >();
    if ( dir->find( name ) )
        throw Error( EADDRINUSE );

    if ( sd->address() )
        throw Error( EINVAL );

    dir->create( std::move( name ), sd );
    sd->address( std::move( address ) );
}

void Manager::connect( int sockfd, const Socket::Address &address ) {
    auto sd = getSocket( sockfd );

    Node model = resolveAddress( address );

    sd->connected( sd, model );
}

int Manager::accept( int sockfd, Socket::Address &address )
{
    Node partner = getSocket( sockfd )->accept();
    address = partner->as< Socket >()->address();

    return _getFileDescriptor( partner, flags::Open::NoFlags );
}

Node Manager::resolveAddress( const Socket::Address &address ) {
    Node item = findDirectoryItem( address.value() );

    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().isSocket() )
        throw Error( ECONNREFUSED );
    _checkGrants( item, Mode::WUSER );
    return item;
}

Node Manager::findDirectoryItem( __dios::String name, bool followSymLinks ) {
    return _findDirectoryItem( std::move( name ), followSymLinks, []( Node ){} );
}
template< typename I >
Node Manager::_findDirectoryItem( __dios::String name, bool followSymLinks, I itemChecker ) {
    if ( name.size() > PATH_LIMIT )
        throw Error( ENAMETOOLONG );
    name = path::normalize( name );
    Node current = _root;
    if ( path::isRelative( name ) )
        current = currentDirectory();

    Node item = current;
    __dios::Queue< __dios::String > q( path::splitPath< __dios::Deque< __dios::String > >( name ) );
    __dios::Set< SymLink * > loopDetector;
    while ( !q.empty() ) {
        if ( !current->mode().isDirectory() )
            throw Error( ENOTDIR );

        _checkGrants( current, Mode::XUSER );

        Directory *dir = current->as< Directory >();

        {
            auto d = utils::make_defer( [&]{ q.pop(); } );
            const auto &subFolder = q.front();
            if ( subFolder.empty() )
                continue;
            if ( subFolder.size() > FILE_NAME_LIMIT )
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
            SymLink *sl = item->as< SymLink >();

            if ( !loopDetector.insert( sl ).second )
                throw Error( ELOOP );

            __dios::Queue< __dios::String > _q( path::splitPath< __dios::Deque< __dios::String > >( sl->target() ) );
            while ( !q.empty() ) {
                _q.emplace( std::move( q.front() ) );
                q.pop();
            }
            q.swap( _q );
            if ( path::isAbsolute( sl->target() ) ) {
                current = _root;
                item = _root;
            }
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

std::pair< Node, __dios::String > Manager::_findDirectoryOfFile( __dios::String name ) {
    name = path::normalize( name );

    if ( name.size() > PATH_LIMIT )
        throw Error( ENAMETOOLONG );

    __dios::String pathname;
    std::tie( pathname, name ) = path::splitFileName( name );
    Node item = findDirectoryItem( pathname );

    if ( !item )
        throw Error( ENOENT );

    if ( !item->mode().isDirectory() )
        throw Error( ENOTDIR );
    _checkGrants( item, Mode::XUSER );
    return { item, name };
}

int Manager::_getFileDescriptor( Node node, Flags< flags::Open > flags, int lowEdge )
{
    return _getFileDescriptor( fs::make_shared< FileDescriptor >( node, flags ), lowEdge );
}

int Manager::_getFileDescriptor( std::shared_ptr< FileDescriptor > f, int lowEdge /* = 0*/ )
{
    int i = 0;

    if ( lowEdge < 0 || lowEdge >= FILE_DESCRIPTOR_LIMIT )
        throw Error( EINVAL );

    if ( lowEdge >= int( _proc->_openFD.size() ) )
        _proc->_openFD.resize( lowEdge + 1 );

    for ( auto &fd : utils::withOffset( _proc->_openFD, lowEdge ) )
    {
        if ( !fd )
        {
            fd = f;
            return i;
        }
        ++i;
    }

    if ( _proc->_openFD.size() >= FILE_DESCRIPTOR_LIMIT )
        throw Error( ENFILE );

    _proc->_openFD.push_back( f );
    return i;
}

void Manager::_insertSnapshotItem( const SnapshotFS &item ) {

    switch( item.type ) {
    case Type::File:
        createNodeAt( CURRENT_DIRECTORY, item.name, item.mode, item.content, item.length );
        break;
    case Type::Directory:
    case Type::Pipe:
    case Type::Socket:
        createNodeAt( CURRENT_DIRECTORY, item.name, item.mode );
        break;
    case Type::SymLink:
        createNodeAt( CURRENT_DIRECTORY, item.name, item.mode, item.content );
        break;
    default:
        break;
    }
}

void Manager::initializeFromSnapshot( const _VM_Env *env ) {
    __dios::Map<uint64_t,__dios::String> inodeMap;

    for ( ; env->key; env++ ) {
        __dios::String key( env->key );

        if ( key.find( ".name" ) != std::string::npos )
        {
            char name[env->size + 1];
            std::copy( env->value, env->value + env->size, name );
            name[env->size] = 0;

            ++env; //stat
            const __vmutil_stat * statInfo( reinterpret_cast<const __vmutil_stat*>( env->value ) );

            auto inMap = inodeMap.find( statInfo->st_ino );
            if ( inMap != inodeMap.end( ) ) {
                // detect hard links
                createHardLinkAt( CURRENT_DIRECTORY, name, CURRENT_DIRECTORY, inMap->second.c_str(), 0 );
                continue;
            }
            inodeMap.insert( std::pair<uint64_t, __dios::String>( statInfo->st_ino, __dios::String( name ) ) );

            ++env; //value
            Mode mode( statInfo->st_mode );
            if( mode.isFile( ) ) {
                createNodeAt( CURRENT_DIRECTORY, name, statInfo->st_mode, env->value, env->size );
            } else if( mode.isDirectory( ) ) {
                createNodeAt( CURRENT_DIRECTORY, name, statInfo->st_mode );
            } else if( mode.isLink( ) ) {
                char value[env->size + 1];
                std::copy( env->value, env->value + env->size, value );
                value[env->size] = 0;
                createSymLinkAt( CURRENT_DIRECTORY, name, value );
            }
        }
     }
}

void Manager::_checkGrants( Node inode, mode_t grant ) const {
    if ( ( inode->mode() & grant ) != grant )
        throw Error( EACCES );
}

void Manager::_chmod( Node inode, mode_t mode ) {
    inode->mode() =
        ( inode->mode() & ~Mode::CHMOD ) |
        ( mode & Mode::CHMOD );
}

} // namespace fs
} // namespace __dios
