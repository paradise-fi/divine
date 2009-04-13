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
    uint16_t size:15;
    uint16_t permanent:1;
};

struct Blob
{
    char *ptr;

    Blob() : ptr( 0 ) {}

    template< typename A >
    Blob( A &a, int size ) {
        assert_leq( 1, size );
        ptr = a.allocate( allocationSize( size ) );
        header().size = size;
        header().permanent = 0;
    }

    explicit Blob( int size ) {
        Allocator< char > a;
        assert_leq( 1, size );
        ptr = a.allocate( allocationSize( size ) );
        header().size = size;
        header().permanent = 0;
    }

    explicit Blob( BlobHeader *s ) : ptr( (char*) s ) {}
    explicit Blob( char *s, bool s_is_data = false )
        : ptr( s_is_data ? s - sizeof( BlobHeader ) : s ) {}

    template< typename A >
    void free( A &a ) {
        if ( !header().permanent )
            a.deallocate( ptr, allocationSize( size() ) );
    }

    void free() {
        Allocator< char > a;
        free( a );
    }

    bool valid() const
    {
        return ptr;
    }

    BlobHeader &header() {
        return *reinterpret_cast< BlobHeader * >( pointer() );
    }

    const BlobHeader &header() const {
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

    template< typename O >
    void write32( O o ) const
    {
        std::copy( pointer32(), pointer32() + allocationSize( size() ) / 4, o );
    }

    template< typename Alloc, typename In >
    In read32( Alloc *a, In i )
    {
        int hdr_cnt = allocationSize( 0 ) / 4;
        int32_t hdr[ hdr_cnt ];
        std::copy( i, i + hdr_cnt, hdr );
        int size = reinterpret_cast< BlobHeader * >( hdr )->size;

        ptr = a->allocate( allocationSize( size ) );
        In end = i + allocationSize( size ) / 4;
        std::copy( i, end, pointer32() );
        return end;
    }

    void setSize( size_t size )
    {
        assert( size <= 0x3FFF );
        header().size = size;
    }

    void clear( char pattern = 0 ) {
        std::fill( data(), data() + size(), pattern );
    }

    size_t size() const
    {
        return header().size;
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

    int32_t *pointer32() const
    {
        return reinterpret_cast< int32_t * >( ptr );
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

        assert_leq( from, to );

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
