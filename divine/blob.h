// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net

// #include <cstdint> requires C++0x : - (
#include <stdint.h>
#include <cstring> // size_t ... d'oh
#include <wibble/test.h> // for assert*
#ifndef BLOB_NO_HASH
#include <divine/hash.h>
#endif

#ifndef DIVINE_BLOB_H
#define DIVINE_BLOB_H

namespace divine {

typedef uint32_t hash_t;

struct BlobHeader {
    uint16_t size:14;
    uint16_t permanent:1;
    uint16_t heap:1;
    uint16_t align; // ...
};

/**
 * A pointer to an adorned memory area. Basically an array of bytes which knows
 * its size and allows convenient (albeit type-unsafe) access to contents. The
 * array can be up to 2^14 (16k) bytes, the header is 2 bytes. Copies of the
 * Blob object share the array (i.e. this really behaves as a pointer). Users
 * are expected to manage associated memory explicitly (no reference counting
 * is done, for performance reasons).
 *
 * This structure also provides convenient hashing and comparison. The equality
 * operator (==) is implemented in terms of the array contents, not in terms of
 * pointer equality. The hash provided is due to Jenkins, a design which should
 * be both fast to compute and well-behaved. Both hashing and comparison can be
 * limited to a contiguous range within the whole array.
 */
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
        header().heap = 0;
    }

    explicit Blob( int size ) {
        assert_leq( 1, size );
        ptr = new char[ allocationSize( size ) ];
        header().size = size;
        header().permanent = 0;
        header().heap = 1;
    }

    explicit Blob( BlobHeader *s ) : ptr( (char*) s ) {}
    explicit Blob( char *s, bool s_is_data = false )
        : ptr( s_is_data ? s - sizeof( BlobHeader ) : s ) {}

    template< typename A >
    void free( A &a ) {
        if ( !valid() )
            return;
        assert( !header().heap );
        if ( !header().permanent )
            a.deallocate( ptr, allocationSize( size() ) );
    }

    void free() {
        if ( !valid() )
            return;
        assert( header().heap );
        if ( !header().permanent )
            delete[] ptr;
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
    O write32( O o ) const
    {
        return std::copy( pointer32(),
                          pointer32() + allocationSize( size() ) / 4, o );
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

    void clear( int from = 0, int to = 0, char pattern = 0 ) {
        if ( to == 0 )
            to = size();
        std::fill( data() + from, data() + to, pattern );
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

#ifndef BLOB_NO_HASH
    hash_t hash() const
    {
        return hash( 0, size() );
    }

    hash_t hash( int from, int to ) const
    {
        uint32_t len, total;
        const char *ptr;

        if ( !valid() || from == to )
            return 0;

        assert_leq( from, to );

        ptr = data() + from;
        len = to - from;
        total = len;

        return jenkins3( ptr, len, 0 );
    }
#endif

};

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
