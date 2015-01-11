#ifndef _FS_DESCRIPTOR_H_
#define _FS_DESCRIPTOR_H_

namespace divine {
namespace fs {

struct FileDescriptor {

    FileDescriptor() :
        _flags( flags::Open::NoFlags ),
        _offset( 0 )
    {}

    FileDescriptor( Ptr dirItem, Flags< flags::Open > fl, size_t offset = 0 ) :
        _directoryItem( dirItem ),
        _file( dirItem->as< FileItem >()->object() ),
        _flags( fl ),
        _offset( offset )
    {}

    FileDescriptor( const FileDescriptor & ) = default;
    FileDescriptor( FileDescriptor && ) = default;
    FileDescriptor &operator=( const FileDescriptor & ) = default;

    long long read( void *buf, size_t length ) {
        if ( !_flags.has( flags::Open::Read ) )
            throw Error( EBADF );
        if ( _offset >= _file->size() || length == 0 )
            return 0;

        char *dst = reinterpret_cast< char * >( buf );
        if ( !_file->read( dst, _offset, length ) )
            throw Error( EBADF );

        _offset += length;
        return length;
    }

    long long write( const void *buf, size_t length ) {
        if ( !_flags.has( flags::Open::Write ) )
            throw Error( EBADF );

        const char *src = reinterpret_cast< const char * >( buf );
        if ( !_file->write( src, _offset, length ) )
            throw Error( EBADF );

        _offset += length;
        return length;
    }

    FSItem &file() {
        return *_file;
    }

    size_t &offset() {
        return _offset;
    }

    size_t size() {
        return _file->size();
    }

    explicit operator bool() const {
        return bool( _file );
    }

    void close() {
        _file.reset();
        _flags = flags::Open::NoFlags;
        _offset = 0;
    }

    ConstPtr directoryItem() const {
        return _directoryItem;
    }

private:
    Ptr _directoryItem;
    std::shared_ptr< FSItem > _file;
    Flags< flags::Open > _flags;
    size_t _offset;
};

} // namespace fs
} // namespace divine

#endif
