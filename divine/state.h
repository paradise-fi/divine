// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net

#include <stdint.h>
#include <wibble/sfinae.h> // for Unit
#include <divine/pool.h>
#include <divine/legacy/system/state.hh>
#include <divine/blob.h>

#ifndef DIVINE_STATE_H
#define DIVINE_STATE_H

namespace divine {

using wibble::Unit;

// from src/common/hash_function.cc
#define MIX(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8);  \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5);  \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

typedef uint32_t hash_t;

template< typename T >
struct AllocSize {
    static const size_t size = sizeof( T );
};

template<>
struct AllocSize< Unit > {
    static const size_t size = 0;
};

template< typename Extension >
struct State
{
    static const int header = 2*sizeof( uint16_t ) + sizeof( char ** );
    static const int ext = AllocSize< Extension >::size;
    static const int extra = header + ext;

    char *ptr;
    State() : ptr( 0 ) {}
    State( const divine::state_t &s )
        : ptr( s.ptr ? s.ptr - extra : 0 )
    {
        assert( s.size == size() );
    }

    State( char *s, bool data = false )
        : ptr( data ? s - extra : s ) {}

    divine::state_t state()
    {
        divine::state_t s;
        s.size = size();
        s.ptr = data();
        return s;
    }

    bool valid() const
    {
        return ptr;
    }

    uint16_t flags() const
    {
        return *( reinterpret_cast< uint16_t * >( pointer() ) + 1 );
    }

    enum Flag { DeleteFlag = 1, FirstUserFlag = 2 };

    void setDeleteFlag() { setFlags( flags() | DeleteFlag ); }
    void clearDeleteFlag() { setFlags( flags() & ~DeleteFlag ); }
    bool hasDeleteFlag() { return flags() & DeleteFlag; }

    void setFlags( uint16_t f )
    {
        *( reinterpret_cast< uint16_t * >( pointer() ) + 1 ) = f;
    }

    // TODO get rid of the redundancy

    State duplicate( Pool &pool ) const
    {
        State r;
        r.ptr = pool.alloc( allocationSize( size() ) );
        r.setSize( size() );
        r.setFlags( 0 );
        std::copy( data(), data() + size(), r.data() );
        std::fill( reinterpret_cast< char * >( &r.extension() ),
                   reinterpret_cast< char * >( &r.extension() ) + ext, 0 );
        return r;
    }

    State duplicate() const
    {
        State r;
        r.ptr = new char[ allocationSize( size() ) ];
        r.setSize( size() );
        r.setFlags( 0 );
        std::copy( data(), data() + size(), r.data() );
        std::fill( reinterpret_cast< char * >( &r.extension() ),
                   reinterpret_cast< char * >( &r.extension() ) + ext, 0 );
        return r;
    }

    static State allocate( Pool &pool, size_t size )
    {
        State r;
        r.ptr = pool.alloc( allocationSize( size ) );
        memset( r.ptr, 0, allocationSize( size ) );
        r.setSize( size );
        assert( r.size() == size );
        assert( r.flags() == 0 );
        return r;
    }

    static State allocate( size_t size )
    {
        State r;
        r.ptr = new char[ allocationSize( size ) ];
        memset( r.ptr, 0, allocationSize( size ) );
        r.setSize( size );
        assert( r.size() == size );
        assert( r.flags() == 0 );
        return r;
    }

    void deallocate( Pool &pool )
    {
        pool.free( pointer(), allocationSize( size() ) );
    }

    void setSize( size_t size )
    {
        assert( size <= 0x3FFF );
        *reinterpret_cast< uint16_t * >( pointer() ) = size;
    }

    size_t size() const
    {
        return (*reinterpret_cast< uint16_t * >( pointer() )) & 0x3FFF;
    }

    static size_t allocationSize( size_t size )
    {
        size_t bytes = size + extra;
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    Extension &extension() {
        return *reinterpret_cast< Extension * >( pointer() + header );
    }

    char *data() const
    {
        return pointer() ? pointer() + extra : 0;
    }

    intptr_t *intptrPointer() const
    {
        return reinterpret_cast< intptr_t * >( pointer() );
    }

    char *pointer() const
    {
        return ptr;
    }

    bool operator==( const State &cb ) const
    {
        const State &ca = *this;
        if ( ca.size() != cb.size() )
            return false;
        int b = 0, e = ca.size();
        while ( b <= e - 4 ) {
            if ( *reinterpret_cast< uint32_t * >( ca.data() + b ) !=
                 *reinterpret_cast< uint32_t * >( cb.data() + b ) )
                return false;
            b += 4;
        }
        while ( b < e ) {
            if ( ca.data()[b] != cb.data()[b] )
                return false;
            ++b;
        }
        return true;
    }

    bool operator!=( const State &cb ) const\
    {
        return !operator==( cb );
    }

    hash_t hash( size_t sz = 0 ) const
    {
        // jenkins hash from common/hash_function.cc

        uint32_t a, b, c, len, total;
        const char *ptr;

        if ( !valid() )
            return 0;

        if ( sz == 0 )
            sz = size();
        else
            sz = std::min( sz, size() );

        a = b = 0x9e3779b9; // magic
        c = sz;
        len = sz;
        ptr = data();
        total = len;

        while (len >= 12)
        {
            a += *reinterpret_cast< const uint32_t * >( ptr );
            b += *reinterpret_cast< const uint32_t * >( ptr + 4 );
            b += *reinterpret_cast< const uint32_t * >( ptr + 8 );
            MIX(a,b,c);
            ptr += 12; len -= 12;
        }

        c += total;
        // handle the remaining bytes (there are at most 11 left)
        switch(len)
        {
        case 11: c += *reinterpret_cast< const uint32_t * >( ptr + 8 )
                 & uint32_t( 0x00FFFFFF ); goto word;
        case 10: c += *reinterpret_cast< const uint32_t * >( ptr + 8 )
                 & uint32_t( 0x0000FFFF ); goto word;
        case  9: c += *reinterpret_cast< const uint32_t * >( ptr + 8 )
                 & uint32_t( 0x000000FF );
            /* the first byte of c is reserved for the length */

        word:
        case  8: b += *reinterpret_cast< const uint32_t * >( ptr + 4 ); goto word2;
        case  7: b += *reinterpret_cast< const uint32_t * >( ptr + 4 )
                 & uint32_t( 0x00FFFFFF ); goto word2;
        case  6: b += *reinterpret_cast< const uint32_t * >( ptr + 4 )
                 & uint32_t( 0x0000FFFF ); goto word2;
        case  5: b += *reinterpret_cast< const uint32_t * >( ptr + 4 )
                 & uint32_t( 0x000000FF );

        word2:
        case  4: a += *reinterpret_cast< const uint32_t * >( ptr + 0 ); goto done;
        case  3: a += *reinterpret_cast< const uint32_t * >( ptr + 0 )
                 & uint32_t( 0x00FFFFFF ); goto done;
        case  2: a += *reinterpret_cast< const uint32_t * >( ptr + 0 )
                 & uint32_t( 0x0000FFFF ); goto done;
        case  1: a += *reinterpret_cast< const uint32_t * >( ptr + 0 )
                 & uint32_t( 0x000000FF );
        case 0:
        done:;
        }

        MIX(a,b,c);

        return c;
    }

};

template< typename T >
inline divine::state_t legacy_state( T s )
{
    return s.state();
}

template<>
inline divine::state_t legacy_state< Blob >( Blob b )
{
    divine::state_t s;
    s.size = b.size();
    s.ptr = b.data();
    return s;
}

#undef MIX

}

#endif
