// -*- C++ -*- (c) 2015 Jiří Weiser

#include <memory>
#include <dirent.h>

#include "utils.h"
#include "constants.h"
#include "inode.h"

#ifndef _FS_DESCRIPTOR_H_
#define _FS_DESCRIPTOR_H_

namespace __dios {
namespace fs {

struct FileDescriptor
{
    FileDescriptor( Node inode, Flags< flags::Open > fl, size_t offset = 0 ) :
        _inode( inode ),
        _flags( fl ),
        _offset( offset )
    {
        inode->open( *this );
    }

    ~FileDescriptor()
    {
        _inode->close( *this );
    }

    FileDescriptor( FileDescriptor && ) noexcept = default;
    FileDescriptor &operator=( FileDescriptor && ) noexcept = default;

    long long read( void *buf, size_t length )
    {
        if ( !_inode )
            throw Error( EBADF );
        if ( !_flags.has( flags::Open::Read ) )
            throw Error( EBADF );

        if ( length == 0 )
            return 0;

        if ( _flags.has( flags::Open::NonBlock ) && !_inode->canRead() )
            throw Error( EAGAIN );

        char *dst = reinterpret_cast< char * >( buf );
        if ( !_inode->read( dst, _offset, length ) )
            throw Error( EBADF );

        _offset += length;
        return length;
    }

    long long write( const void *buf, size_t length ) {
        if ( !_inode )
            throw Error( EBADF );
        if ( !_flags.has( flags::Open::Write ) )
            throw Error( EBADF );

        if ( _flags.has( flags::Open::NonBlock ) && !_inode->canWrite() )
            throw Error( EAGAIN );

        if ( _flags.has( flags::Open::Append ) )
            _offset = _inode->size();

        const char *src = reinterpret_cast< const char * >( buf );
        if ( !_inode->write( src, _offset, length ) )
            throw Error( EBADF );

        _offset += length;
        return length;
    }

    size_t offset() const {
        if ( _inode->mode().isFifo() )
            throw Error( EPIPE );
        return _offset;
    }

    void offset( size_t off ) {
        _offset = off;
    }

    size_t size() {
        return _inode ? _inode->size() : 0;
    }

    explicit operator bool() const {
        return bool( _inode );
    }

    Node inode() {
        return _inode;
    }

    const Node inode() const {
        return _inode;
    }

    void close() {
        _inode->close( *this );
        _inode.reset();
        _flags = flags::Open::NoFlags;
        _offset = 0;
    }

    Flags< flags::Open > flags() const {
        return _flags;
    }
    Flags< flags::Open > &flags() {
        return _flags;
    }

protected:
    Node _inode;
    Flags< flags::Open > _flags;
    size_t _offset;
};

} // namespace fs
} // namespace __dios

#endif
