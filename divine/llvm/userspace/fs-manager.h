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

#ifndef _FS_MANAGER_H_
#define _FS_MANAGER_H_

namespace divine {
namespace fs {

struct FSManager {

    FSManager() :
        FSManager( true )
    {
        _standardIO[ 0 ]->assign( new RegularFile() );
    }

    FSManager( const char *in, size_t length ) :
        FSManager( true )
    {
        _standardIO[ 0 ]->assign( new RegularFile( in, length ) );
    }

    explicit FSManager( std::initializer_list< SnapshotFS > items ) :
        FSManager()
    {
        for ( const auto &item : items )
            _insertSnapshotItem( item );
    }

    FSManager( const char *in, size_t length, std::initializer_list< SnapshotFS > items ) :
        FSManager( in, length )
    {
        for ( const auto &item : items )
            _insertSnapshotItem( item );
    }

    Node findDirectoryItem( utils::String name, bool followSymLinks = true );

    void createDirectory( utils::String name, unsigned mode );
    void createHardLink( utils::String name, const utils::String &target );
    void createSymLink( utils::String name, utils::String target );
    int createFile( utils::String name, unsigned mode );

    void accessAt( int dirfd, utils::String name, Flags< flags::Access > mode, Flags< flags::At > fl );
    int openFileAt( int dirfd, utils::String name, Flags< flags::Open > fl, unsigned mode );
    void closeFile( int fd );
    int duplicate( int oldfd );
    int duplicate2( int oldfd, int newfd );
    std::shared_ptr< FileDescriptor > &getFile( int fd );

    void removeFile( utils::String name );
    void removeDirectory( utils::String name );
    void removeAt( int dirfd, utils::String name, flags::At fl );

    off_t lseek( int fd, off_t offset, Seek whence );

    template< typename DirPre, typename DirPost, typename File >
    void traverseDirectoryTree( const utils::String &root, DirPre pre, DirPost post, File file ) {
        Node current = findDirectoryItem( root );
        if ( !current || !current->mode().isDirectory() )
            return;
        if ( pre( root ) ) {
            for ( auto &i : *current->data()->as< Directory >() ) {

                if ( i.name() == "." || i.name() == ".." )
                    continue;

                utils::String path = utils::joinPath( root, i.name() );
                if ( i.inode()->mode().isDirectory() )
                    traverseDirectoryTree( path, pre, post, file );
                else
                    file( path );
            }

            post( root );
        }
    }

    Node currentDirectory() {
        return _currentDirectory.lock();
    }

    void changeDirectory( utils::String path );
    void changeDirectory( int dirfd );

    unsigned umask() const {
        return _umask;
    }
    void umask( unsigned mask ) {
        _umask = 0777 & mask;
    }

private:
    Node _root;
    WeakNode _currentDirectory;
    std::array< Node, 2 > _standardIO;
    utils::Vector< std::shared_ptr< FileDescriptor > > _openFD;

    unsigned short _umask;

    FSManager( bool );// private default ctor

    template< typename... Args >
    void _createFile( utils::String name, unsigned mode, Node *file, Args &&... args );

    std::pair< Node, utils::String > _findDirectoryOfFile( utils::String name );
    int _getFileDescriptor( std::shared_ptr< FileDescriptor > f );
    void _insertSnapshotItem( const SnapshotFS &item );

    void _checkGrants( Node inode, unsigned grant ) const;

};



} // namespace fs
} // namespace divine

#endif
