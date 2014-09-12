// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net

#include <cstdint>

#include <wibble/test.h> // for assert*
#include <wibble/string.h>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/rpc.h>

#ifndef DIVINE_BLOB_H
#define DIVINE_BLOB_H

namespace divine {

/* NB. The previous contents of the blob that's read is *not released*. */
template< typename B >
bitstream_base< B > &operator>>( bitstream_base< B > &bs, Blob &b )
{
    int size = 0;
    bs >> size;

    if ( !size ) {
        b = Blob();
        return bs;
    }

    b = bs.pool->allocate( size );

    ASSERT_LEQ( bs.pool->size( b ), bs.size() * 4 );

    char *begin = bs.pool->dereference( b );
    const char *end = begin + bs.pool->size( b );
    uint32_t *ptr = reinterpret_cast< uint32_t * >( begin );
    while ( ptr < reinterpret_cast< const uint32_t * >( end ) ) {
        bs >> *ptr;
        ++ptr;
    }

    return bs;
}

template< typename B >
bitstream_base< B > &operator<<( bitstream_base< B > &bs, Blob b )
{
    ASSERT( bs.pool );
    if ( !bs.pool->valid( b ) )
        return bs << 0;

    const int size = bs.pool->size( b );
    bs << size;
    const char *begin = bs.pool->dereference( b );
    const char *end = begin + size;
    const uint32_t *ptr = reinterpret_cast< const uint32_t * >( begin );
    while ( ptr < reinterpret_cast< const uint32_t * >( end ) ) {
        bs << *ptr;
        ++ptr;
    }
    return bs;
}

}

namespace wibble {
namespace str {

template<>
inline std::string fmt( const divine::UnBlob &b ) {
    divine::bitstream bs;
    bs.pool = &b.p;
    bs << b.b;
    bs.bits.pop_front(); /* remove size */
    return fmt( bs.bits );
}

}
}

namespace divine {
inline std::string fmtblob( Pool &p, Blob b ) {
    return wibble::str::fmt( UnBlob( p, b ) );
}
}

#endif
