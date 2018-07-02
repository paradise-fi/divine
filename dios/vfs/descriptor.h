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
    FileDescriptor( Node inode, OFlags fl, size_t offset = 0 ) :
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
        if ( length == 0 )
            return 0;

        if ( _flags.nonblock() && !_inode->canRead() )
            return error( EAGAIN ), -1;

        char *dst = reinterpret_cast< char * >( buf );
        if ( !_inode->read( dst, _offset, length ) )
            return error( EBADF ), -1;

        _offset += length;
        return length;
    }

    long long write( const void *buf, size_t length )
    {
        if ( _flags.nonblock() && !_inode->canWrite() )
            return error( EAGAIN ), -1;

        if ( _flags.has( O_APPEND ) )
            _offset = _inode->size();

        const char *src = reinterpret_cast< const char * >( buf );
        if ( !_inode->write( src, _offset, length ) )
            return error( EBADF ), -1;

        _offset += length;
        return length;
    }

    size_t offset() const { return _offset; }
    void offset( size_t off ) { _offset = off; }
    size_t size() { return _inode ? _inode->size() : 0; }
    Node inode() const { return _inode; }
    OFlags flags() const { return _flags; }
    OFlags &flags() { return _flags; }
    explicit operator bool() const { return bool( _inode );}

    void close()
    {
        _inode->close( *this );
        _inode.reset();
        _flags = 0;
        _offset = 0;
    }

protected:
    Node _inode;
    OFlags _flags;
    size_t _offset;
};

} // namespace fs
} // namespace __dios

#endif
