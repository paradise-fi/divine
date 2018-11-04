// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2015 Jiří Weiser
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <algorithm>
#include <signal.h>
#include <dios.h>
#include <dios/sys/memory.hpp>
#include <dios/sys/trace.hpp>
#include <rst/common.h>
#include <sys/fault.h>
#include <sys/trace.h>

#include "utils.h"
#include <dios/vfs/inode.hpp>
#include <dios/vfs/fd.hpp>
#include "storage.h"

#define FS_CHOICE_GOAL          0

namespace __dios::fs
{

struct Socket : INode
{
    struct Address
    {
        Address() :
            _anonymous( true ),
            _valid( false )
        {}

        explicit Address( std::string_view value, bool anonymous = false ) :
            _value( value ),
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
        return receive( buffer, length, flags::Message::NoFlags, dummy );
    }

    const Address &address() const { return _address; }
    bool bind( std::string_view addr ) override { _address = Address( addr ); return true; }

    virtual Socket *peer() const = 0;

    virtual bool canReceive( size_t ) const = 0;
    virtual bool canConnect() const = 0;

    virtual void listen( int ) = 0;
    virtual void addBacklog( Node ) = 0;

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

}

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

namespace __dios::fs {

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
            return error( EINVAL ), nullptr;

        // progress or deadlock
        if ( _backlog.empty() )
            __vm_cancel();

        Node result( std::move( _backlog.front() ) );
        _backlog.pop();
        return result;
    }

    bool connect( Node self, Node remote, bool allocateNew )
    {
        if ( _peer )
            return error( EISCONN ), false;

        SocketStream *m = remote->as< SocketStream >();

        if ( allocateNew )
        {
            if ( !m->canConnect() )
                return error( ECONNREFUSED ), false;

            _peer.reset( new( __dios::nofail ) SocketStream( self ) );
            _peer->mode( ACCESSPERMS );
            m->addBacklog( _peer );
        }
        else
        {
            _peer = std::move( remote );
            m->_peer = std::move( self );
        }

        return true;
    }

    bool connect( Node self, Node remote ) override
    {
        return connect( std::move( self ), std::move( remote ), true );
    }

    void addBacklog( Node incomming ) override {
        if ( int( _backlog.size() ) == _limit )
            throw Error( ECONNREFUSED );
        _backlog.push( std::move( incomming ) );
    }

    bool canRead() const override {
        return !_stream.empty();
    }

    bool canWrite( int size, Node ) const override
    {
        return peer() && peer()->canReceive( size );
    }

    bool canReceive( size_t amount ) const override {
        return _stream.size() + amount <= _stream.capacity();
    }
    bool canConnect() const override {
        return _passive && !closed();
    }

    bool write( const char *buffer, size_t, size_t &length, Node ) override
    {
        if ( !peer() )
            return error( ENOTCONN ), false;

        if ( !peer()->mode().user_write() )
            return error( EACCES ), false;

        return peer()->fillBuffer( buffer, length );
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

    bool canWrite( int size, Node remote ) const override
    {
        if ( !remote )
            remote = _defaultRecipient.lock();
        if ( !remote )
            return true;
        return !remote->as< Socket >()->canReceive( size );
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
        return error( EOPNOTSUPP ), nullptr;
    }

    void addBacklog( Node ) override {
    }

    bool connect( Node, Node defaultRecipient ) override
    {
        _defaultRecipient = defaultRecipient;
        return true;
    }

    bool write( const char *buffer, size_t, size_t &length, Node remote ) override
    {
        if ( !remote )
            remote = _defaultRecipient.lock();

        if ( !remote )
            return error( EDESTADDRREQ ), false;

        if ( !remote->mode().user_write() )
            return error( EACCES ), false;

        Socket *socket = remote->as< Socket >();
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

}
