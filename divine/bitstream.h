// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <vector>
#include <deque>
#include <string>

#include <divine/blob.h>
#include <divine/pool.h>

#ifndef DIVINE_BITSTREAM_H
#define DIVINE_BITSTREAM_H

namespace divine {
namespace bitstream_impl {

template< typename Bits >
struct base {
    Bits bits;
    Pool *pool;

    void clear() { bits.clear(); }
    bool empty() { return bits.empty(); }
    size_t size() { return bits.size(); }
    void shift() { bits.pop_front(); }
    uint32_t &front() { return bits.front(); }
    void push( uint32_t i ) { bits.push_back( i ); }
    base() : pool( 0 ) {}
};

struct block : std::vector< uint32_t > {};

template<> struct base< block > {
    block bits;
    int offset;
    Pool *pool;

    void clear() { bits.clear(); offset = 0; }
    void maybeclear() {
        if ( offset == bits.size() )
            clear();
    }

    bool empty() {
        maybeclear();
        return bits.empty();
    }
    size_t size() { return bits.size(); }
    void shift() {
        ++ offset;
        maybeclear();
    }
    void push( uint32_t i ) { bits.push_back( i ); }
    uint32_t &front() { return bits[ offset ]; }
    base() : offset( 0 ), pool( 0 ) {}
};

template< typename B, typename X > struct container {};
template< typename B, typename T > struct container< B, std::vector< T > > {
    typedef base< B > stream; };
template< typename B, typename T > struct container< B, std::deque< T > > {
    typedef base< B > stream; };
template< typename B > struct container< B, std::string > {
    typedef base< B > stream; };

template< typename B, typename X > struct integer {};
template< typename B > struct integer< B, bool > { typedef base< B > stream; };
template< typename B > struct integer< B, char > { typedef base< B > stream; };
template< typename B > struct integer< B, int8_t > { typedef base< B > stream; };
template< typename B > struct integer< B, int16_t > { typedef base< B > stream; };
template< typename B > struct integer< B, int32_t > { typedef base< B > stream; };
template< typename B > struct integer< B, uint8_t > { typedef base< B > stream; };
template< typename B > struct integer< B, uint16_t > { typedef base< B > stream; };
template< typename B > struct integer< B, uint32_t > { typedef base< B > stream; };

template< typename B, typename X > struct int64 {};
template< typename B > struct int64< B, uint64_t > { typedef base< B > stream; };
template< typename B > struct int64< B, int64_t > { typedef base< B > stream; };

template< typename B, typename X >
typename container< B, X >::stream &operator<<( base< B > &bs, X x ) {
    bs << x.size();
    for ( typename X::const_iterator i = x.begin(); i != x.end(); ++i )
        bs << *i;
    return bs;
}

template< typename B, typename T >
typename integer< B, T >::stream &operator<<( base< B > &bs, T i ) {
    bs.push( i );
    return bs;
}

template< typename B, typename T >
typename int64< B, T >::stream &operator<<( base< B > &bs, T i ) {
    union { uint64_t x64; struct { uint32_t a; uint32_t b; } x32; };
    x64 = i;
    bs << x32.a << x32.b;
    return bs;
}

template< typename B, typename T1, typename T2 >
base< B > &operator<<( base< B > &bs, std::pair< T1, T2 > i ) {
    return bs << i.first << i.second;
}

template< typename B, typename T >
typename integer< B, T >::stream &operator>>( base< B > &bs, T &x ) {
    x = bs.front();
    bs.shift();
    return bs;
}

template< typename B, typename T >
typename int64< B, T >::stream &operator>>( base< B > &bs, T &i ) {
    union { uint64_t x64; struct { uint32_t a; uint32_t b; } x32; };
    bs >> x32.a >> x32.b;
    i = x64;
    return bs;
}

template< typename B, typename X >
typename container< B, X >::stream &operator>>( base< B > &bs, X &x ) {
    int size;
    bs >> size;
    for ( int i = 0; i < size; ++ i ) {
        typename X::value_type tmp;
        bs >> tmp;
        x.push_back( tmp );
    }
    return bs;
}

template< typename B, typename T1, typename T2 >
base< B > &operator>>( base< B > &bs, std::pair< T1, T2 > &i ) {
    return bs >> i.first >> i.second;
}

/* NB. The previous contents of the blob that's read is *not released*. */
template< typename B >
base< B > &operator>>( base< B > &bs, Blob &blob )
{
    int size, off = 0;
    bs >> size;

    if ( !size ) {
        blob = Blob();
        return bs;
    }

    if ( bs.pool )
        blob = Blob( *bs.pool, size );
    else
        blob = Blob( size );

    while ( off < blob.size() ) {
        bs >> blob.get< uint32_t >( off );
        off += 4;
    }

    return bs;
}

template< typename B >
base< B > &operator<<( base< B > &bs, Blob blob )
{
    if ( !blob.valid() )
        return bs << 0;

    bs << blob.size();
    int off = 0;
    while ( off < blob.size() ) {
        bs << blob.get< uint32_t >( off );
        off += 4;
    }
    return bs;
}

}

typedef bitstream_impl::base< std::deque< uint32_t > > bitstream;
typedef bitstream_impl::base< bitstream_impl::block > bitblock;

}

#endif
