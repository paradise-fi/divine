// -*- C++ -*- (c) 2015 Jiří Weiser

#include <memory>

#include "fs-utils.h"
#include "fs-file.h"
#include "fs-constants.h"
#include "fs-directory.h"

#ifndef _FS_DESCRIPTOR_H_
#define _FS_DESCRIPTOR_H_

namespace divine {
namespace fs {

struct FileDescriptor {

    FileDescriptor() :
        _inode( nullptr ),
        _flags( flags::Open::NoFlags ),
        _offset( 0 )
    {}

    FileDescriptor( Node inode, Flags< flags::Open > fl, size_t offset = 0 ) :
        _inode( inode ),
        _flags( fl ),
        _offset( offset )
    {}

    FileDescriptor( const FileDescriptor & ) = default;
    FileDescriptor( FileDescriptor && ) = default;
    FileDescriptor &operator=( const FileDescriptor & ) = default;

    virtual ~FileDescriptor() = default;

    bool canRead() const {
        if ( !_inode )
            throw Error( EBADF );

        File *file = _inode->data()->as< File >();
        if ( !file )
            throw Error( EBADF );

        return file->canRead();
    }
    bool canWrite() const {
        if ( !_inode )
            throw Error( EBADF );

        File *file = _inode->data()->as< File >();
        if ( !file )
            throw Error( EBADF );

        return file->canWrite();
    }

    virtual long long read( void *buf, size_t length ) {
        if ( !_inode )
            throw Error( EBADF );
        if ( !_flags.has( flags::Open::Read ) )
            throw Error( EBADF );

        File *file = _inode->data()->as< File >();
        if ( !file )
            throw Error( EBADF );
        if ( _flags.has( flags::Open::NonBlock ) && !file->canRead() )
            throw Error( EAGAIN );

        char *dst = reinterpret_cast< char * >( buf );
        if ( !file->read( dst, _offset, length ) )
            throw Error( EBADF );

        _setOffset( _offset + length );
        return length;
    }

    virtual long long write( const void *buf, size_t length ) {
        if ( !_inode )
            throw Error( EBADF );
        if ( !_flags.has( flags::Open::Write ) )
            throw Error( EBADF );

        File *file = _inode->data()->as< File >();
        if ( !file )
            throw Error( EBADF );
        if ( _flags.has( flags::Open::NonBlock ) && !file->canWrite() )
            throw Error( EAGAIN );

        if ( _flags.has( flags::Open::Append ) )
            _offset = file->size();

        const char *src = reinterpret_cast< const char * >( buf );
        if ( !file->write( src, _offset, length ) )
            throw Error( EBADF );

        _setOffset( _offset + length );
        return length;
    }

    size_t offset() const {
        return _offset;
    }
    virtual void offset( size_t off ) {
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
    virtual void _setOffset( size_t off ) {
        _offset = off;
    }

    Node _inode;
    Flags< flags::Open > _flags;
    size_t _offset;
};

struct PipeDescriptor : FileDescriptor {

    PipeDescriptor() :
        FileDescriptor()
    {}

    PipeDescriptor( Node inode, Flags< flags::Open > fl, bool wait = false ) :
        FileDescriptor( inode, fl )
    {
        Pipe *pipe = inode->data()->as< Pipe >();

        /// TODO: enable detection of deadlock
        if ( fl.has( flags::Open::Read ) && fl.has( flags::Open::Write ) )
            __dios_fault( vm::Fault::Assert, "Pipe is opened both for reading and writing" );
        else if ( fl.has( flags::Open::Read ) ) {
            pipe->assignReader();
            while ( wait && !pipe->writer() ) {
                FS_MAKE_INTERRUPT();
            }
        }
        else if ( fl.has( flags::Open::Write ) ) {
            pipe->assignWriter();
            while ( wait && !pipe->reader() ) {
                FS_MAKE_INTERRUPT();
            }
        }
    }

    PipeDescriptor( const PipeDescriptor & ) = default;
    PipeDescriptor( PipeDescriptor && ) = default;
    PipeDescriptor &operator=( const PipeDescriptor & ) = default;


    ~PipeDescriptor() {
        if ( _inode && _flags.has( flags::Open::Read ) ) {
            Pipe *pipe = _inode->data()->as< Pipe >();

            pipe->releaseReader();
        }
    }

    long long read( void *buf, size_t length ) override {
        if ( !_inode )
            throw Error( EBADF );
        if ( !_flags.has( flags::Open::Read ) )
            throw Error( EBADF );

        File *file = _inode->data()->as< File >();
        if ( !file )
            throw Error( EBADF );

        if ( length == 0 )
            return 0;

        char *dst = reinterpret_cast< char * >( buf );
        if ( !file->read( dst, _offset, length ) )
            throw Error( EBADF );

        _offset += length;
        return length;
    }

    void offset( size_t off ) override {
        throw Error( EPIPE );
    }
protected:
    void _setOffset( size_t ) override {
    }
};

struct DirectoryDescriptor {

    DirectoryDescriptor( Node inode, int fd ) :
        _fd( fd )
    {
        if ( !inode->mode().isDirectory() )
            throw Error( ENOTDIR );

        Directory *dir = inode->data()->as< Directory >();

        _items.reserve( dir->size() );
        for ( const auto &item : *dir ) {
            _items.emplace_back( item );
        }
        rewind();
    }

    void rewind() {
        _position = _items.begin();
    }
    void next() {
        ++_position;
    }

    const DirectoryItemLabel *get() const {
        if ( _position == _items.end() )
            return nullptr;
        return &*_position;
    }

    void seek( long off ) {
        rewind();
        _position += off;
    }

    long tell() const {
        return long( _position - _items.begin() );
    }

    int fd() const {
        return _fd;
    }

private:

    utils::Vector< DirectoryItemLabel > _items;
    utils::Vector< DirectoryItemLabel >::const_iterator _position;
    int _fd;
};

struct SocketDescriptor : FileDescriptor {

    SocketDescriptor( Node inode, Flags< flags::Open > fl ) :
        FileDescriptor( std::move( inode ), fl | flags::Open::Read | flags::Open::Write ),
        _socket( _inode->data()->as< Socket >() )
    {}

    ~SocketDescriptor() {
        _socket->close();
    }

    void listen( int backlog ) {
        _socket->listen( backlog );
    }

    void connected( Node partner ) {
        _socket->connected( _inode, std::move( partner ) );
    }

    Node accept() {
        return _socket->accept();
    }

    void address( const Socket::Address &addr ) {
        _socket->address( addr );
    }

    Socket &peer() {
        return _socket->peer();
    }

    const Socket::Address &address() const {
        return _socket->address();
    }

    size_t send( const char *buffer, size_t length, Flags< flags::Message > fls ) {
        if ( _flags.has( flags::Open::NonBlock ) && !_socket->canWrite() )
            throw Error( EAGAIN );

        if ( this->flags().has( flags::Open::NonBlock ) )
            fls |= flags::Message::DontWait;

        _socket->send( buffer, length, fls );
        return length;
    }

    size_t sendTo( const char *buffer, size_t length, Flags< flags::Message > fls, Node node ) {
        if ( _flags.has( flags::Open::NonBlock ) && !_socket->canWrite() )
            throw Error( EAGAIN );

        if ( this->flags().has( flags::Open::NonBlock ) )
            fls |= flags::Message::DontWait;

        _socket->sendTo( buffer, length, fls, node );
        return length;
    }

    size_t receive( char *buffer, size_t length, Flags< flags::Message > fls, Socket::Address &address ) {
        if ( _flags.has( flags::Open::NonBlock ) && !_socket->canRead() )
            throw Error( EAGAIN );

        if ( this->flags().has( flags::Open::NonBlock ) )
            fls |= flags::Message::DontWait;

        _socket->receive( buffer, length, fls, address );
        return length;
    }


private:
    Socket *_socket;
};

} // namespace fs
} // namespace divine

#endif
