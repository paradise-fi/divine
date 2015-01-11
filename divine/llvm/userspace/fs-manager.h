#include <tuple>
#include <memory>
#include <algorithm>
#include <utility>
#include <array>

#include "fs-utils.h"
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
        _standardIO{ {
            { nullptr, "", Grants::ALL, std::make_shared< File >() },
            { nullptr, "", Grants::ALL, std::make_shared< FileNull >() } 
        } },
        _openFD{
            std::make_shared< FileDescriptor >( &_standardIO[ 0 ], flags::Open::Read ),// stdin
            std::make_shared< FileDescriptor >( &_standardIO[ 1 ], flags::Open::Write ),// stdout
            std::make_shared< FileDescriptor >( &_standardIO[ 1 ], flags::Open::Write )// stderr
        }
    {}

    FSManager( const char *in, size_t length ) :
        _standardIO{ {
            { nullptr, "", Grants::ALL, std::make_shared< File >( in, length ) },
            { nullptr, "", Grants::ALL, std::make_shared< FileNull >() }
        } },
        _openFD{
            std::make_shared< FileDescriptor >( &_standardIO[ 0 ], flags::Open::Read ),// stdin
            std::make_shared< FileDescriptor >( &_standardIO[ 1 ], flags::Open::Write ),// stdout
            std::make_shared< FileDescriptor >( &_standardIO[ 1 ], flags::Open::Write )// stderr
        }
    {}

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

    Ptr findDirectoryItem( const utils::String &name, bool followSymLinks = true );

    void createDirectory( utils::String name, unsigned mode );
    void createHardLink( utils::String name, const utils::String &target );
    void createSymLink( utils::String name, utils::String target );
    int createFile( utils::String name, unsigned mode );

    void access( utils::String name, Flags< flags::Access > mode );
    int openFile( utils::String name, Flags< flags::Open > fl, unsigned mode );
    void closeFile( int fd );
    int duplicate( int oldfd );
    int duplicate2( int oldfd, int newfd );
    std::shared_ptr< FileDescriptor > &getFile( int fd );

    void removeFile( utils::String name, Directory *current = nullptr );
    void removeDirectory( utils::String name, Directory *current = nullptr );

    off_t lseek( int fd, off_t offset, Seek whence );

    template< typename DirPre, typename DirPost, typename File >
    void traverseDirectoryTree( const utils::String &root, DirPre pre, DirPost post, File file ) {
        Directory *current = _findDirectory( root );
        if ( !current )
            return;
        if ( pre( root ) ) {
            for ( auto &i : *current ) {

                if ( i->name() == "." || i->name() == ".." )
                    continue;

                utils::String path = utils::joinPath( root, i->name() );
                if ( i->isDirectory() )
                    traverseDirectoryTree( path, pre, post, file );
                else
                    file( path );
            }

            post( root );
        }
    }

private:
    Directory _root;
    std::array< FileItem, 2 > _standardIO;
    utils::Vector< std::shared_ptr< FileDescriptor > > _openFD;

    template< typename... Args >
    void _createFile( utils::String name, unsigned mode, Ptr *file, Args &&... args );

    std::pair< Directory *, utils::String > _findDirectoryOfFile( utils::String name );
    Directory *_findDirectory( const utils::String &path );
    int _getFileDescriptor( std::shared_ptr< FileDescriptor > f );
    void _insertSnapshotItem( const SnapshotFS &item );

    void _checkGrants( const Grantsable *item, unsigned grant ) const;

};



} // namespace fs
} // namespace divine

#endif
