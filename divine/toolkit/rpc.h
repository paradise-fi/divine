// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net

#include <brick-rpc.h>

#include <divine/toolkit/pool.h>

#ifndef DIVINE_TOOLKIT_RPC_H
#define DIVINE_TOOLKIT_RPC_H

namespace divine {

namespace rpc = brick::rpc;

struct WithPool {
    Pool *pool;
    WithPool( Pool &p ) : pool( &p ) {}
    WithPool() : pool( nullptr ) {}
};

template< typename X >
using bitstream_base = rpc::_impl::base< X, WithPool >;

using bitstream = bitstream_base< std::deque< uint32_t > >;
using bitblock = bitstream_base< rpc::_impl::block >;

template< typename B >
bitstream_base< B > &operator<<( bitstream_base< B > &bs, wibble::Unit ) {
    return bs;
}

template< typename B >
bitstream_base< B > &operator>>( bitstream_base< B > &bs, wibble::Unit ) {
    return bs;
}

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

#endif
