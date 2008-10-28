// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net

// #include <cstdint> requires C++0x : - (
#include <stdint.h>
#include <cstring> // size_t ... d'oh
#include <wibble/test.h> // for assert*
#include <divine/pool.h> // for Allocator

#ifndef DIVINE_BLOB_H
#define DIVINE_BLOB_H

namespace divine {

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

struct BlobHeader {
    uint16_t size;
};

struct Blob
{
    char *ptr;

    Blob() : ptr( 0 ) {}

    template< typename A >
    Blob( A *a, int size ) {
        ptr = a->allocate( allocationSize( size ) );
        setSize( size );
    }

    explicit Blob( int size ) {
        Allocator< char > a;
        ptr = a.allocate( allocationSize( size ) );
        setSize( size );
    }

    explicit Blob( BlobHeader *s ) : ptr( (char*) s ) {}
    explicit Blob( char *s, bool s_is_data = false )
        : ptr( s_is_data ? s - sizeof( BlobHeader ) : s ) {}

    template< typename A >
    void free( A* a ) {
        a->deallocate( ptr, allocationSize( size() ) );
    }

    void free() {
        Allocator< char > a;
        free( &a );
    }

    bool valid() const
    {
        return ptr;
    }

    BlobHeader &header() {
        return *reinterpret_cast< BlobHeader * >( pointer() );
    }

    template< typename T >
    T &get( int off = 0 ) {
        return *reinterpret_cast< T * >( data() + off );
    }

    void copyTo( Blob &where ) const
    {
        assert_eq( where.size(), size() );
        std::copy( data(), data() + size(), where.data() );
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
        size_t bytes = size + sizeof( BlobHeader );
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    char *data() const
    {
        return pointer() ? pointer() + sizeof( BlobHeader ) : 0;
    }

    char *pointer() const
    {
        return ptr;
    }

    bool operator<( const Blob &b ) const {
        int cmp = compare( b, 0, size() );
        return cmp < 0;
    }

    bool operator==( const Blob &b ) const {
        return compare( b, 0, size() ) == 0;
    }

    int compare( const Blob &cb, int b, int e ) const
    {
        assert( b <= e );
        const Blob &ca = *this;
        if ( !(b < ca.size() && b < cb.size()) )
            return ca.size() - cb.size();
        if ( !(e <= ca.size() && e <= cb.size()) )
            return ca.size() - cb.size();

        while ( b <= e - signed(sizeof(intptr_t)) ) {
            intptr_t x = *reinterpret_cast< intptr_t * >( ca.data() + b ),
                     y = *reinterpret_cast< intptr_t * >( cb.data() + b );
            if ( x < y )
                return -1;
            if ( y < x )
                return 1;
            b += sizeof( intptr_t );
        }

        while ( b < e ) {
            char x = ca.data()[b], y = cb.data()[b];
            if ( x < y )
                return -1;
            if ( y < x )
                return 1;
            ++b;
        }

        return 0;
    }

    hash_t hash() const
    {
        return hash( 0, size() );
    }

    hash_t hash( int from, int to ) const
    {
        // jenkins hash from common/hash_function.cc

        uint32_t a, b, c, len, total;
        const char *ptr;

        if ( !valid() || from == to )
            return 0;

        assert( from <= to );

        a = b = 0x9e3779b9; // magic
        len = c = to - from;
        ptr = data() + from;
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

#undef MIX

template< typename N >
inline N unblob( const N &n ) {
    return n;
}

template< typename N >
inline N unblob( Blob b ) {
    return b.get< N >();
}

template<>
inline Blob unblob( Blob b ) {
    return b;
}


}

#endif
