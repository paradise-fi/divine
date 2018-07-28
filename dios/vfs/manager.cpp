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
    _root( new( __dios::nofail ) Directory() ),
    _currentDirectory( _root ),
    _standardIO{ { make_shared< StandardInput >(), nullptr, nullptr } }
{
    _root->mode( S_IFDIR | ACCESSPERMS );
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


ssize_t Manager::readLinkAt( int dirfd, __dios::String name, char *buf, size_t count ) {
    REMEMBER_DIRECTORY( dirfd, name );

    Node inode = findDirectoryItem( std::move( name ), false );
    if ( !inode )
        throw Error( ENOENT );
    if ( !inode->mode().is_link() )
        throw Error( EINVAL );

    SymLink *sl = inode->as< SymLink >();
    auto target = sl->target();
    auto realLength = std::min( target.size(), count );
    std::copy( target.begin(), target.begin() + realLength, buf );
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

    if ( ( ( mode & R_OK ) && !item->mode().user_read() ) ||
         ( ( mode & W_OK ) && !item->mode().user_write() ) ||
         ( ( mode & X_OK ) && !item->mode().user_exec() ) )
        throw Error( EACCES );
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
    Node node( new ( nofail ) Pipe() );
    node->mode( S_IRWXU | S_IFIFO );
    auto fd1 = _getFileDescriptor( node, O_RDONLY | O_NONBLOCK );
    auto fd2 =  _getFileDescriptor( node, O_WRONLY | O_NONBLOCK );
    return { fd1, fd2 };
}

off_t Manager::lseek( int fd, off_t offset, Seek whence ) {
    auto f = getFile( fd );
    if ( f->inode()->mode().is_fifo() )
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

void Manager::getCurrentWorkingDir( char *buff, size_t size ) {
    if ( buff == nullptr )
        throw Error( EFAULT );
    if ( size == 0 )
        throw Error( EINVAL );
    __dios::String path = "";
    Node originalDirectory = currentDirectory( );

    Node current = originalDirectory;
    while( current != _root ) {
        Directory *dir = current->as< Directory >( );
        if ( !dir )
            break;
        path = "/" + String( dir->name() ) + path;
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
    if ( !item->mode().is_dir() )
        throw Error( ENOTDIR );
    _checkGrants( item, S_IXUSR );

    _currentDirectory = item;
}

void Manager::changeDirectory( int dirfd ) {
    auto fd = getFile( dirfd );
    if ( !fd )
        throw Error( EBADF );
    Node item = fd->inode();
    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().is_dir() )
        throw Error( ENOTDIR );
    _checkGrants( item, S_IXUSR );

    _currentDirectory = item;
}

int Manager::socket( SocketType type, OFlags fl )
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
    socket->mode( ACCESSPERMS | S_IFSOCK );

    return _getFileDescriptor( socket, fl );
}

std::pair< int, int > Manager::socketpair( SocketType type, OFlags fl )
{
    if ( type != SocketType::Stream )
        throw Error( EOPNOTSUPP );

    SocketStream *cl = new( __dios::nofail ) SocketStream;
    Node client( cl );
    client->mode( ACCESSPERMS | S_IFSOCK );

    Node server( new( __dios::nofail ) SocketStream );
    server->mode( ACCESSPERMS | S_IFSOCK );

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

    dir->create( std::move( name ), sd, false );
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

    return _getFileDescriptor( partner, 0 );
}

Node Manager::resolveAddress( const Socket::Address &address ) {
    Node item = findDirectoryItem( address.value() );

    if ( !item )
        throw Error( ENOENT );
    if ( !item->mode().is_socket() )
        throw Error( ECONNREFUSED );
    _checkGrants( item, S_IWUSR );
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
        if ( !current->mode().is_dir() )
            throw Error( ENOTDIR );

        _checkGrants( current, S_IXUSR );

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

        if ( item->mode().is_dir() )
            current = item;
        else if ( item->mode().is_link() && ( followSymLinks || !q.empty() ) ) {
            SymLink *sl = item->as< SymLink >();

            if ( !loopDetector.insert( sl ).second )
                throw Error( ELOOP );

            __dios::Queue< String > _q( path::splitPath< Deque< String > >( String( sl->target() ) ) );
            while ( !q.empty() ) {
                _q.emplace( std::move( q.front() ) );
                q.pop();
            }
            q.swap( _q );
            if ( path::isAbsolute( String( sl->target() ) ) ) {
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

    if ( !item->mode().is_dir() )
        throw Error( ENOTDIR );
    _checkGrants( item, S_IXUSR );
    return { item, name };
}

int Manager::_getFileDescriptor( Node node, OFlags flags, int lowEdge )
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

void Manager::_checkGrants( Node inode, Mode grant ) const
{
    if ( ( inode->mode() & grant ) != grant )
        throw Error( EACCES );
}

} // namespace fs
} // namespace __dios
