// -*- C++ -*- (c) 2015 Jiří Weiser

#include <memory>
#include <dirent.h>

#include "utils.h"
#include "file.h"
#include "constants.h"
#include "directory.h"

#ifndef _FS_DESCRIPTOR_H_
#define _FS_DESCRIPTOR_H_

namespace __dios {
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

    bool canRead() const
    {
        if ( !_inode )
            throw Error( EBADF );

        return _inode->canRead();
    }

    bool canWrite() const
    {
        if ( !_inode )
            throw Error( EBADF );

        return _inode->canWrite();
    }

    virtual long long read( void *buf, size_t length )
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

    virtual long long write( const void *buf, size_t length ) {
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

    virtual size_t offset() const {
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
        Pipe *pipe = inode->as< Pipe >();

        /// TODO: enable detection of deadlock
        if ( fl.has( flags::Open::Read ) && fl.has( flags::Open::Write ) )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened both for reading and writing" );
        else if ( fl.has( flags::Open::Read ) )
        {
            if ( wait && !pipe->writer() )
               __vm_cancel();
            pipe->assignReader();
        }
        else if ( fl.has( flags::Open::Write ) )
        {
            if ( wait && !pipe->reader() )
               __vm_cancel();
            pipe->assignWriter();
        }
    }

    PipeDescriptor( const PipeDescriptor & ) = default;
    PipeDescriptor( PipeDescriptor && ) = default;
    PipeDescriptor &operator=( const PipeDescriptor & ) = default;

    ~PipeDescriptor() {
        if ( _inode && _flags.has( flags::Open::Read ) ) {
            Pipe *pipe = _inode->as< Pipe >();
            pipe->releaseReader();
        }
    }

    void offset( size_t ) override
    {
        throw Error( EPIPE );
    }
};

struct DirectoryDescriptor : FileDescriptor
{
    DirectoryDescriptor( Node inode, Flags< flags::Open > fl, size_t offset = 0 )
        : FileDescriptor(inode, fl, offset)
    {
        if ( !inode->mode().isDirectory() )
            throw Error( ENOTDIR );
    }
};

struct SocketDescriptor : FileDescriptor {

    SocketDescriptor( Node inode, Flags< flags::Open > fl ) :
        FileDescriptor( std::move( inode ), fl | flags::Open::Read | flags::Open::Write ),
        _socket( _inode->as< Socket >() )
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
} // namespace __dios

#endif
