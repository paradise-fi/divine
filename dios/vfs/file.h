// -*- C++ -*- (c) 2015 Jiří Weiser

#include <algorithm>
#include <signal.h>
#include <dios.h>
#include <dios/sys/memory.hpp>
#include <dios/sys/trace.hpp>
#include <rst/common.h>
#include <sys/fault.h>
#include <sys/trace.h>

#include "utils.h"
#include "inode.h"
#include "descriptor.h"
#include "storage.h"

#define FS_CHOICE_GOAL          0

#ifndef _FS_FILE_H_
#define _FS_FILE_H_

namespace __dios {
namespace fs {

struct SymLink : INode
{
    SymLink( __dios::String target ) :
        _target( std::move( target ) )
    {
        if ( _target.size() > PATH_LIMIT )
            throw Error( ENAMETOOLONG );
    }

    size_t size() const override { return _target.size(); }
    const __dios::String &target() const { return _target; }

private:
    __dios::String _target;
};

struct RegularFile : INode
{
    RegularFile( const char *content, size_t size ) :
        _snapshot( bool( content ) ),
        _size( content ? size : 0 ),
        _roContent( content )
    {}

    RegularFile() :
        _snapshot( false ),
        _size( 0 ),
        _roContent( nullptr )
    {}

    RegularFile( const RegularFile &other ) = default;
    RegularFile( RegularFile &&other ) = default;
    RegularFile &operator=( RegularFile ) = delete;

    size_t size() const override { return _size; }
    bool canRead() const override { return true; }
    bool canWrite() const override { return true; }

    bool read( char *buffer, size_t offset, size_t &length ) override
    {
        if ( offset >= _size ) {
            length = 0;
            return true;
        }
        const char *source = _isSnapshot() ?
                          _roContent + offset :
                          _content.data() + offset;
        if ( offset + length > _size )
            length = _size - offset;
        std::copy( source, source + length, buffer );
        return true;
    }

    bool write( const char *buffer, size_t offset, size_t &length ) override
    {
        if ( _isSnapshot() )
            _copyOnWrite();

        if ( _content.size() < offset + length )
            resize( offset + length );

        std::copy( buffer, buffer + length, _content.begin() + offset );
        return true;
    }

    void clear() override
    {
        if ( !_size )
            return;

        _snapshot = false;
        resize( 0 );
    }

    void resize( size_t length )
    {
        _content.resize( length );
        _size = _content.size();
    }

private:

    bool _isSnapshot() const {
        return _snapshot;
    }

    void _copyOnWrite() {
        const char *roContent = _roContent;
        _content.resize( _size );

        std::copy( roContent, roContent + _size, _content.begin() );
        _snapshot = false;
    }

    bool _snapshot;
    size_t _size;
    const char *_roContent;
    __dios::Vector< char > _content;
};

struct VmTraceFile : INode
{
    bool canWrite() const override { return true; }

    __attribute__(( __annotate__( "divine.debugfn" ) ))
    bool write( const char *buffer, size_t, size_t &length ) override
    {
        if ( buffer[ length - 1 ] == 0 )
            __dios::traceInternal( 0, "%s", buffer );
        else
        {
            char buf[ length + 1 ];
            std::copy( buffer, buffer + length, buf );
            buf[ length ] = 0;
            __dios::traceInternal( 0, "%s", buf );
        }
        return true;
    }
};

struct VmBuffTraceFile : INode
{
    bool canWrite() const override { return true; }

    __debugfn void do_write( const char *data, size_t &length ) noexcept
    {
        auto &buf = get_debug().trace_buf[ abstract::weaken( __dios_this_task() ) ];
        buf.insert( buf.length(), data, length );
        auto nl = buf.find_last_of( "\n" );
        if ( nl != std::string::npos )
        {
            __dios::traceInternal( 0, "%s", buf.substr( 0, nl ).c_str() );
            buf.erase( 0, nl + 1 );
        }
        get_debug().persist();
        get_debug().persist_buffers();
    }

    __debugfn void do_flush() noexcept
    {
        for ( auto &b : get_debug().trace_buf )
        {
            if ( !b.second.empty() )
                __dios::traceInternal( 0, "%s", b.second.c_str() );
            b.second.clear();
        }
        get_debug().persist();
        get_debug().persist_buffers();
    }

    bool write( const char *data, size_t, size_t & length ) override
    {
        do_write( data, length );
        return true;
    }

    ~VmBuffTraceFile() { do_flush(); }
};

struct StandardInput : INode
{
    StandardInput() :
        _content( nullptr ),
        _size( 0 )
    {}

    StandardInput( const char *content, size_t size ) :
        _content( content ),
        _size( size )
    {}

    size_t size() const override { return _size; }

    bool canRead() const override
    {
        // simulate user drinking coffee
        if ( _size )
            return FS_CHOICE( 2 ) == FS_CHOICE_GOAL;
        return false;
    }

    bool read( char *buffer, size_t offset, size_t &length ) override
    {
        if ( offset >= _size ) {
            length = 0;
            return true;
        }
        const char *source = _content + offset;
        if ( offset + length > _size )
            length = _size - offset;
        std::copy( source, source + length, buffer );
        return true;
    }

private:
    const char *_content;
    size_t _size;
};

struct Pipe : INode
{
    Pipe() :
        _stream( PIPE_SIZE_LIMIT ),
        _reader( false ),
        _writer( false )
    {}

    Pipe( bool r, bool w ) :
        _stream( PIPE_SIZE_LIMIT ),
        _reader( r ),
        _writer( w )
    {}

    size_t size() const override { return _stream.size(); }
    bool canRead() const override { return size() > 0; }
    bool canWrite() const override { return size() < PIPE_SIZE_LIMIT; }

    void open( FileDescriptor &fd ) override
    {
        __dios_assert( fd.inode().get() == this );

        if ( fd.flags().read() && fd.flags().write() )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened both for reading and writing" );

        if ( fd.flags().read() )
        {
            if ( fd.flags().has( O_FIFO_WAIT ) && !writer() )
               __vm_cancel();
            assignReader();
        }

        if ( fd.flags().write() )
        {
            if ( fd.flags().has( O_FIFO_WAIT ) && !reader() )
               __vm_cancel();
            assignWriter();
        }
    }

    void close( FileDescriptor &fd ) override
    {
        __dios_assert( fd.inode().get() == this );
        if ( fd.flags().read() )
            releaseReader();
    }

    bool read( char *buffer, size_t, size_t &length ) override
    {
        if ( length == 0 )
            return true;

        // progress or deadlock
        while ( ( length = _stream.pop( buffer, length ) ) == 0 )
            __vm_cancel();

        return true;
    }

    bool write( const char *buffer, size_t, size_t &length ) override
    {
        if ( !_reader ) {
            raise( SIGPIPE );
            throw Error( EPIPE );
        }

        // progress or deadlock
        while ( ( length = _stream.push( buffer, length ) ) == 0 )
            __vm_cancel();

        return true;
    }

    void clear() override { throw Error( EINVAL ); }
    void releaseReader() { _reader = false; }
    bool reader() const { return _reader; }
    bool writer() const { return _writer; }

    void assignReader()
    {
        if ( _reader )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened for reading again." );
        _reader = true;
    }

    void assignWriter()
    {
        if ( _writer )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened for writing again." );
        _writer = true;
    }

private:
    storage::Stream _stream;
    bool _reader;
    bool _writer;
};

struct Socket : INode
{
    struct Address
    {
        Address() :
            _anonymous( true ),
            _valid( false )
        {}

        explicit Address( __dios::String value, bool anonymous = false ) :
            _value( std::move( value ) ),
            _anonymous( anonymous ),
            _valid( true )
        {}
        Address( const Address & ) = default;
        Address( Address && ) = default;

        Address &operator=( Address other ) {
            swap( other );
            return *this;
        }

        const __dios::String &value() const {
            return _value;
        }

        bool anonymous() const {
            return _anonymous;
        }

        bool valid() const {
            return _valid;
        }

        size_t size() const {
            return _value.size();
        }

        explicit operator bool() const {
            return _valid;
        }

        void swap( Address &other ) {
            using std::swap;

            swap( _value, other._value );
            swap( _anonymous, other._anonymous );
            swap( _valid, other._valid );
        }

        bool operator==( const Address &other ) const {
            return
                _valid == other._valid &&
                _anonymous == other._anonymous &&
                _value == other._value;
        }

        bool operator!=( const Address &other ) const {
            return !operator==( other );
        }

    private:
        __dios::String _value;
        bool _anonymous;
        bool _valid;
    };

    void open( FileDescriptor &fd ) override
    {
        fd.flags().clear( O_RDONLY | O_WRONLY );
        fd.flags().set( O_RDWR );
    }

    bool read( char *buffer, size_t, size_t &length ) override
    {
        Address dummy;
        receive( buffer, length, flags::Message::NoFlags, dummy );
        return true;
    }

    bool write( const char *buffer, size_t, size_t &length ) override
    {
        return send( buffer, length, flags::Message::NoFlags );
    }

    const Address &address() const { return _address; }
    void address( Address addr ) { _address.swap( addr ); }

    virtual Socket *peer() const = 0;

    virtual bool canReceive( size_t ) const = 0;
    virtual bool canConnect() const = 0;

    virtual void listen( int ) = 0;
    virtual Node accept() = 0;
    virtual void addBacklog( Node ) = 0;
    virtual void connected( Node, Node ) = 0;

    virtual bool send( const char *, size_t &, LegacyFlags< flags::Message > ) = 0;
    virtual bool sendTo( const char *, size_t &, LegacyFlags< flags::Message >, Node ) = 0;

    virtual bool receive( char *, size_t &, LegacyFlags< flags::Message >, Address & ) = 0;

    virtual bool fillBuffer( const char*, size_t & ) = 0;
    virtual bool fillBuffer( const Address &, const char *, size_t & ) = 0;

    bool closed() const {
        return _closed;
    }

    void close( FileDescriptor & ) override
    {
        _closed = true;
        abort();
    }
protected:
    virtual void abort() = 0;
private:
    Address _address;
    bool _closed = false;
};

} // namespace fs
} // namespace __dios

namespace std {
template<>
struct hash< ::__dios::fs::Socket::Address > {
    size_t operator()( const ::__dios::fs::Socket::Address &a ) const {
        return hash< __dios::String >()( a.value() );
    }
};

template<>
inline void swap( ::__dios::fs::Socket::Address &lhs, ::__dios::fs::Socket::Address &rhs ) {
    lhs.swap( rhs );
}

} // namespace std

namespace __dios {
namespace fs {

struct SocketStream : Socket {

    SocketStream() :
        _peer( nullptr ),
        _stream( 1024 ),
        _passive( false ),
        _limit( 0 )
    {}

    SocketStream( Node partner ) :
        _peer( std::move( partner ) ),
        _stream( 1024 ),
        _passive( true ),
        _limit( 0 )
    {}

    Socket *peer() const override
    {
        return _peer ? _peer->as< Socket >() : nullptr;
    }

    void abort() override { _peer.reset(); }

    void listen( int limit ) override
    {
        _passive = true;
        _limit = limit;
    }

    Node accept() override
    {
        if ( !_passive )
            throw Error( EINVAL );

        // progress or deadlock
        if ( _backlog.empty() )
            __vm_cancel();

        Node result( std::move( _backlog.front() ) );
        _backlog.pop();
        return result;
    }

    void connected( Node self, Node model, bool allocateNew )
    {
        if ( _peer )
            throw Error( EISCONN );

        SocketStream *m = model->as< SocketStream >();

        if ( allocateNew )
        {
            if ( !m->canConnect() )
                throw Error( ECONNREFUSED );

            _peer.reset( new( __dios::nofail ) SocketStream( self ) );
            _peer->mode( Mode::GRANTS );
            m->addBacklog( _peer );
        }
        else
        {
            _peer = std::move( model );
            m->_peer = std::move( self );
        }
    }

    void connected( Node self, Node model ) override {
        connected( std::move( self ), std::move( model ), true );
    }

    void addBacklog( Node incomming ) override {
        if ( int( _backlog.size() ) == _limit )
            throw Error( ECONNREFUSED );
        _backlog.push( std::move( incomming ) );
    }

    bool canRead() const override {
        return !_stream.empty();
    }
    bool canWrite() const override {
        return peer() && peer()->canReceive( 1 );
    }
    bool canReceive( size_t amount ) const override {
        return _stream.size() + amount <= _stream.capacity();
    }
    bool canConnect() const override {
        return _passive && !closed();
    }

    bool send( const char *buffer, size_t &length, LegacyFlags< flags::Message > fls ) override
    {
        if ( !peer() )
            return error( ENOTCONN ), false;

        if ( !peer()->mode().userWrite() )
            return error( EACCES ), false;

        if ( fls.has( flags::Message::DontWait ) && !peer()->canReceive( length ) )
            return error( EAGAIN ), false;

        return peer()->fillBuffer( buffer, length );
    }

    bool sendTo( const char *buffer, size_t &length, LegacyFlags< flags::Message > fls, Node ) override
    {
        return send( buffer, length, fls );
    }

    bool receive( char *buffer, size_t &length, LegacyFlags< flags::Message > fls, Address &address ) override
    {
        if ( !peer() && !closed() )
            return error( ENOTCONN ), false;

        if ( _stream.empty() )
            __vm_cancel();

        if ( fls.has( flags::Message::WaitAll ) && _stream.size() < length )
            __vm_cancel();

        if ( fls.has( flags::Message::Peek ) )
            length = _stream.peek( buffer, length );
        else
            length = _stream.pop( buffer, length );

        address = peer()->address();

        return true;
    }

    bool fillBuffer( const Address &, const char *, size_t & ) override
    {
        return error( EPROTOTYPE ), false;
    }

    bool fillBuffer( const char *buffer, size_t &length ) override
    {
        if ( closed() )
        {
            abort();
            return error( ECONNRESET ), false;
        }

        length = _stream.push( buffer, length );
        return true;
    }

private:
    Node _peer;
    storage::Stream _stream;
    bool _passive;
    __dios::Queue< Node > _backlog;
    int _limit;
};

struct SocketDatagram : Socket {

    SocketDatagram()
    {}

    Socket *peer() const override
    {
        if ( auto dr = _defaultRecipient.lock() )
        {
            SocketDatagram *defRec = dr->as< SocketDatagram >();
            if ( auto self = defRec->_defaultRecipient.lock() )
                if ( self.get() == this )
                    return defRec;
        }
        return nullptr;
    }

    bool canRead() const override {
        return !_packets.empty();
    }

    bool canWrite() const override {
        if ( auto dr = _defaultRecipient.lock() ) {
            return !dr->as< Socket >()->canReceive( 0 );
        }
        return true;
    }

    bool canReceive( size_t ) const override {
        return !closed();
    }

    bool canConnect() const override {
        return false;
    }

    void listen( int ) override {
        throw Error( EOPNOTSUPP );
    }

    Node accept() override {
        throw Error( EOPNOTSUPP );
    }

    void addBacklog( Node ) override {
    }

    void connected( Node, Node defaultRecipient ) override {
        _defaultRecipient = defaultRecipient;
    }

    bool send( const char *buffer, size_t &length, LegacyFlags< flags::Message > fls ) override
    {
        return SocketDatagram::sendTo( buffer, length, fls, _defaultRecipient.lock() );
    }

    bool sendTo( const char *buffer, size_t &length, LegacyFlags< flags::Message >, Node target ) override
    {
        if ( !target )
            return error( EDESTADDRREQ ), false;

        if ( !target->mode().userWrite() )
            return error( EACCES ), false;

        Socket *socket = target->as< Socket >();
        return socket->fillBuffer( address(), buffer, length );
    }

    bool receive( char *buffer, size_t &length, LegacyFlags< flags::Message > fls, Address &address ) override {

        if ( fls.has( flags::Message::DontWait ) && _packets.empty() )
            return error( EAGAIN ), false;

        if  ( _packets.empty() )
            __vm_cancel();

        length = _packets.front().read( buffer, length );
        address = _packets.front().from();
        if ( !fls.has( flags::Message::Peek ) )
            _packets.pop();

        return true;
    }

    bool fillBuffer( const char */*buffer*/, size_t &/*length*/ ) override
    {
        return error( EPROTOTYPE ), false;
    }

    bool fillBuffer( const Address &sender, const char *buffer, size_t &length ) override
    {
        if ( closed() )
            return error( ECONNREFUSED ), false;
        _packets.emplace( sender, buffer, length );
        return true;
    }

    void abort() override {
    }


private:
    struct Packet {

        Packet( Address from, const char *data, size_t length ) :
            _from( std::move( from ) ),
            _data( data, data + length )
        {}

        Packet( const Packet & ) = delete;
        Packet( Packet && ) = default;
        Packet &operator=( Packet other ) {
            swap( other );
            return *this;
        }

        size_t read( char *buffer, size_t max ) const {
            size_t result = std::min( max, _data.size() );
            std::copy( _data.begin(), _data.begin() + result, buffer );
            return result;
        }

        const Address &from() const {
            return _from;
        }

        void swap( Packet &other ) {
            using std::swap;

            swap( _from, other._from );
            swap( _data, other._data );
        }

    private:
        Address _from;
        __dios::Vector< char > _data;
    };

    __dios::Queue< Packet > _packets;
    WeakNode _defaultRecipient;

};

} // namespace fs
} // namespace __dios

#endif
