// -*- C++ -*- (c) 2015 Jiří Weiser

#include <tuple>
#include <memory>
#include <algorithm>
#include <utility>
#include <array>

#include "fs-utils.h"
#include "fs-inode.h"
#include "fs-constants.h"
#include "fs-file.h"
#include "fs-directory.h"
#include "fs-snapshot.h"
#include "fs-descriptor.h"
#include "fs-path.h"

#ifndef _FS_MANAGER_H_
#define _FS_MANAGER_H_

namespace divine {
namespace fs {

struct Manager {

    Manager() :
        Manager( true )
    {
        _standardIO[ 0 ]->assign( new( __dios::nofail ) StandardInput() );
    }

    Manager( const char *in, size_t length ) :
        Manager( true )
    {
        _standardIO[ 0 ]->assign( new( __dios::nofail ) StandardInput( in, length ) );
    }

    explicit Manager( std::initializer_list< SnapshotFS > items ) :
        Manager()
    {
        for ( const auto &item : items )
            _insertSnapshotItem( item );
    }

    Manager( const char *in, size_t length, std::initializer_list< SnapshotFS > items ) :
        Manager( in, length )
    {
        for ( const auto &item : items )
            _insertSnapshotItem( item );
    }

    void setOutputFile(FileTrace trace);

    void setErrFile(FileTrace trace);

    Node findDirectoryItem( __dios::String name, bool followSymLinks = true );

    void createHardLinkAt( int newdirfd, __dios::String name, int olddirfd, const __dios::String &target, Flags< flags::At > fl );
    void createSymLinkAt( int dirfd, __dios::String name, __dios::String target );
    template< typename... Args >
    Node createNodeAt( int dirfd, __dios::String name, mode_t mode, Args &&... args );

    ssize_t readLinkAt( int dirfd, __dios::String name, char *buf, size_t count );

    void accessAt( int dirfd, __dios::String name, Flags< flags::Access > mode, Flags< flags::At > fl );
    int openFileAt( int dirfd, __dios::String name, Flags< flags::Open > fl, mode_t mode );
    void closeFile( int fd );
    int duplicate( int oldfd, int lowEdge = 0 );
    int duplicate2( int oldfd, int newfd );
    std::shared_ptr< FileDescriptor > &getFile( int fd );
    std::shared_ptr< SocketDescriptor > getSocket( int sockfd );

    std::pair< int, int > pipe();

    void removeFile( __dios::String name );
    void removeDirectory( __dios::String name );
    void removeAt( int dirfd, __dios::String name, flags::At fl );

    void renameAt( int newdirfd, __dios::String newpath, int olddirfd, __dios::String oldpath );

    void truncate( Node inode, off_t length );

    off_t lseek( int fd, off_t offset, Seek whence );

    template< typename DirPre, typename DirPost, typename File >
    void traverseDirectoryTree( const __dios::String &root, DirPre pre, DirPost post, File file ) {
        Node current = findDirectoryItem( root );
        if ( !current || !current->mode().isDirectory() )
            return;
        if ( pre( root ) ) {
            for ( auto &i : *current->data()->as< Directory >() ) {

                if ( i.name() == "." || i.name() == ".." )
                    continue;

                __dios::String pathname = path::joinPath( root, i.name() );
                if ( i.inode()->mode().isDirectory() )
                    traverseDirectoryTree( pathname, pre, post, file );
                else
                    file( pathname );
            }

            post( root );
        }
    }

    Node currentDirectory() {
        return _currentDirectory.lock();
    }

    void changeDirectory( __dios::String pathname );
    void changeDirectory( int dirfd );

    void chmodAt( int dirfd, __dios::String name, mode_t mode, Flags< flags::At > fl );
    void chmod( int fd, mode_t mode );

    mode_t umask() const {
        return _umask;
    }
    void umask( mode_t mask ) {
        _umask = Mode::GRANTS & mask;
    }

    DirectoryDescriptor *openDirectory( int fd );
    DirectoryDescriptor *getDirectory( void *descriptor );
    void closeDirectory( void *descriptor );

    int socket( SocketType type, Flags< flags::Open > fl );
    std::pair< int, int > socketpair( SocketType type, Flags< flags::Open > fl );
    void bind( int sockfd, Socket::Address address );
    void connect( int sockfd, const Socket::Address &address );
    int accept( int sockfd, Socket::Address &address );
    Node resolveAddress( const Socket::Address &address );
    bool wasError() const {
        return _error == 0;
    }

    int getError() const {
        return _error;
    }

    void setError(int code) {
        _error = code;
    }

private:
    Node _root;
    int _error;
    WeakNode _currentDirectory;
    std::array< Node, 3 > _standardIO;
    __dios::Vector< std::shared_ptr< FileDescriptor > > _openFD;
    __dios::List< DirectoryDescriptor > _openDD;

    unsigned short _umask;

    Manager( bool );// private default ctor

    std::pair< Node, __dios::String > _findDirectoryOfFile( __dios::String name );

    template< typename I >
    Node _findDirectoryItem( __dios::String name, bool followSymLinks, I itemChecker );

    int _getFileDescriptor( std::shared_ptr< FileDescriptor > f, int lowEdge = 0 );
    void _insertSnapshotItem( const SnapshotFS &item );

    void _checkGrants( Node inode, unsigned grant ) const;

    void _chmod( Node inode, mode_t mode );

};

struct VFS {

    VFS() {
        // __divine_interrupt_mask();
        _manager = new( __dios::nofail ) Manager{};
    }
    VFS( const char *in, size_t length ) {
        // __divine_interrupt_mask();
        _manager = new( __dios::nofail ) Manager{ in, length };
    }
    explicit VFS( std::initializer_list< SnapshotFS > items ) {
        // __divine_interrupt_mask();
        _manager = new( __dios::nofail ) Manager{ items };
    }
    VFS( const char *in, size_t length, std::initializer_list< SnapshotFS > items ) {
        // __divine_interrupt_mask();
        _manager = new( __dios::nofail ) Manager{ in, length, items };
    }
    ~VFS() {
        delete _manager;
    }

    Manager &instance() {
        __FS_assert( _manager );
        return *_manager;
    }

private:
    Manager *_manager;
};

extern VFS vfs;

} // namespace fs
} // namespace divine

#endif
