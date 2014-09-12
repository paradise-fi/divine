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

}

#endif
