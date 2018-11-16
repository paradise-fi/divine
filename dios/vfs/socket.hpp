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
            _anonymous( anonymous ),
            _valid( true )
        {
            _value.assign( value.size(), value.begin(), value.end() );
        }

        Address( const Address & ) = default;
        Address( Address && ) = default;

        Address &operator=( Address other ) {
            swap( other );
            return *this;
        }

        std::string_view value() const { return { _value.begin(), _value.size() }; }

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
        Array< char > _value;
        bool _anonymous;
        bool _valid;
    };

    void open( FileDescriptor &fd ) override
    {
        fd.flags().clear( O_RDONLY | O_WRONLY );
        fd.flags().set( O_RDWR );
        INode::open( fd );
    }

    bool read( char *buffer, size_t, size_t &length ) override
    {
        return !!receive( buffer, length, 0 );
    }

    std::string_view address() override { return _address.value(); }
    bool bind( std::string_view addr ) override { _address = Address( addr ); return true; }

    virtual Socket *sock_peer() const = 0;
    const Address &sock_address() const { return _address; }

    virtual bool canReceive( size_t ) const = 0;
    virtual bool canConnect() const = 0;

    virtual bool addBacklog( Node ) = 0;

    virtual bool fillBuffer( const char*, size_t & ) = 0;
    virtual bool fillBuffer( Node sender, const char *, size_t & ) = 0;

private:
    Address _address;
};

}

namespace std {
template<>
struct hash< ::__dios::fs::Socket::Address > {
    size_t operator()( const ::__dios::fs::Socket::Address &a ) const {
        return hash< std::string_view >()( a.value() );
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

    virtual ~SocketStream() { abort( false ); }
    Node peer() const override { return _peer; }

    SocketStream *sock_peer() const override
    {
        return _peer ? _peer->as< SocketStream >() : nullptr;
    }

    void abort( bool remote )
    {
        if ( _peer && !remote )
            sock_peer()->abort( true );
        _peer = nullptr;
    }

    bool listen( int limit ) override
    {
        _passive = true;
        _limit = limit;
        return true;
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

    bool connect( Node remote, bool allocateNew )
    {
        if ( _peer )
            return error( EISCONN ), false;

        SocketStream *m = remote->as< SocketStream >();

        if ( allocateNew )
        {
            if ( !m->canConnect() )
                return error( ECONNREFUSED ), false;

            _peer = new( nofail ) SocketStream( this );
            _peer->mode( ACCESSPERMS );
            return m->addBacklog( _peer );
        }
        else
        {
            _peer = remote;
            m->_peer = this;
            return true;
        }
    }

    bool connect( Node remote ) override
    {
        return connect( remote, true );
    }

    bool addBacklog( Node incoming ) override
    {
        if ( int( _backlog.size() ) == _limit )
            return error( ECONNREFUSED ), false;
        incoming->bind( address() );
        _backlog.push( std::move( incoming ) );
        return true;
    }

    bool canRead() const override {
        return !_stream.empty();
    }

    bool canWrite( int size, Node ) const override
    {
        return peer() && sock_peer()->canReceive( size );
    }

    bool canReceive( size_t amount ) const override {
        return _stream.size() + amount <= _stream.capacity();
    }
    bool canConnect() const override {
        return _passive;
    }

    bool write( const char *buffer, size_t, size_t &length, Node ) override
    {
        if ( !peer() )
            return error( ENOTCONN ), false;

        if ( !peer()->mode().user_write() )
            return error( EACCES ), false;

        return sock_peer()->fillBuffer( buffer, length );
    }

    Node receive( char *buffer, size_t &length, MFlags flags ) override
    {
        if ( !peer() )
            return error( ENOTCONN ), nullptr;

        if ( _stream.empty() )
            __vm_cancel();

        if ( flags.has( MSG_WAITALL ) && _stream.size() < length )
            __vm_cancel();

        if ( flags.has( MSG_PEEK ) )
            length = _stream.peek( buffer, length );
        else
            length = _stream.pop( buffer, length );

        return peer();
    }

    bool fillBuffer( Node, const char *, size_t & ) override
    {
        return error( EPROTOTYPE ), false;
    }

    bool fillBuffer( const char *buffer, size_t &length ) override
    {
        if ( !peer() )
            return error( ECONNRESET ), false;

        length = _stream.push( buffer, length );
        return true;
    }

private:
    Node _peer;
    storage::Stream _stream;
    bool _passive;
    Queue< Node > _backlog;
    int _limit;
};

struct SocketDatagram : Socket
{

    SocketDatagram()
    {}

    Node peer()  const override
    {
        if ( auto dr = _defaultRecipient )
        {
            SocketDatagram *defRec = dr->as< SocketDatagram >();
            if ( auto self = defRec->_defaultRecipient )
                if ( self == this )
                    return dr;
        }
        return nullptr;
    }

    Socket *sock_peer() const override
    {
        auto p = peer();
        return p ? p->as< Socket >() : nullptr;
    }

    bool canRead() const override {
        return !_packets.empty();
    }

    bool canWrite( int size, Node remote ) const override
    {
        if ( !remote )
            remote = _defaultRecipient;
        if ( !remote )
            return true;
        return !remote->as< Socket >()->canReceive( size );
    }

    bool canReceive( size_t ) const override {
        return true;
    }

    bool canConnect() const override {
        return false;
    }

    bool listen( int ) override
    {
        return error( EOPNOTSUPP ), false;
    }

    Node accept() override {
        return error( EOPNOTSUPP ), nullptr;
    }

    bool addBacklog( Node ) override { return true; }

    bool connect( Node defaultRecipient ) override
    {
        /* FIXME the default recipient might disappear any time */
        _defaultRecipient = defaultRecipient;
        return true;
    }

    bool write( const char *buffer, size_t, size_t &length, Node remote ) override
    {
        if ( !remote )
            remote = _defaultRecipient;

        if ( !remote )
            return error( EDESTADDRREQ ), false;

        if ( !remote->mode().user_write() )
            return error( EACCES ), false;

        Socket *socket = remote->as< Socket >();
        return socket->fillBuffer( this, buffer, length );
    }

    Node receive( char *buffer, size_t &length, MFlags flags ) override
    {
        if ( flags.has( MSG_DONTWAIT ) && _packets.empty() )
            return error( EAGAIN ), nullptr;

        if  ( _packets.empty() )
            __vm_cancel();

        length = _packets.front().read( buffer, length );
        auto peer = _packets.front().from();
        if ( !flags.has( MSG_PEEK ) )
            _packets.pop();

        return peer;
    }

    bool fillBuffer( const char *, size_t &/*length*/ ) override
    {
        return error( EPROTOTYPE ), false;
    }

    bool fillBuffer( Node sender, const char *buffer, size_t &length ) override
    {
        _packets.emplace( sender, buffer, length );
        return true;
    }

private:
    struct Packet {

        Packet( Node from, const char *data, size_t length ) :
            _from( std::move( from ) ),
            _data( length, data, data + length )
        {}

        Packet( const Packet & ) = delete;
        Packet( Packet && ) = default;
        Packet &operator=( Packet other ) {
            swap( other );
            return *this;
        }

        size_t read( char *buffer, size_t max ) const {
            size_t result = std::min( max, size_t( _data.size() ) );
            std::copy( _data.begin(), _data.begin() + result, buffer );
            return result;
        }

        Node from() const {
            return _from;
        }

        void swap( Packet &other ) {
            using std::swap;

            swap( _from, other._from );
            swap( _data, other._data );
        }

    private:
        Node _from;
        Array< char > _data;
    };

    Queue< Packet > _packets;
    Node _defaultRecipient;
};

}
