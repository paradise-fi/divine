// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <vector>
#include <deque>
#include <string>

#include <divine/toolkit/pool.h>

#ifndef DIVINE_BITSTREAM_H
#define DIVINE_BITSTREAM_H

namespace divine {
namespace bitstream_impl {

template< typename Bits >
struct base {
    typedef base< Bits > bitstream;
    Bits bits;
    Pool *pool;

    void clear() { bits.clear(); }
    bool empty() { return bits.empty(); }
    int size() { return bits.size(); }
    void shift() { bits.pop_front(); }
    uint32_t &front() { return bits.front(); }
    void push( uint32_t i ) { bits.push_back( i ); }
    base() : pool( nullptr ) {}
    base( Pool& p ) : pool( &p ) {}
};

struct block : std::vector< uint32_t > {};

template<> struct base< block > {
    typedef base< block > bitstream;
    block bits;
    int offset;
    Pool *pool;

    void clear() { bits.clear(); offset = 0; }
    void maybeclear() {
        if ( offset == int( bits.size() ) )
            clear();
    }

    bool empty() {
        maybeclear();
        return bits.empty();
    }
    int size() { return bits.size() - offset; }
    void shift() {
        ++ offset;
        maybeclear();
    }
    void push( uint32_t i ) { bits.push_back( i ); }
    uint32_t &front() { return bits[ offset ]; }
    base() : offset( 0 ), pool( nullptr ) {}
    base( Pool& p ) : offset( 0 ), pool( &p ) { }
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

template< typename B, std::size_t I, typename... Tp >
inline typename std::enable_if< (I == sizeof...(Tp)), void >::type
writeTuple( base< B > &, std::tuple< Tp... >& ) {}

template< typename B, std::size_t I, typename... Tp >
inline typename std::enable_if< (I < sizeof...(Tp)), void >::type
writeTuple( base< B > &bs, std::tuple< Tp... >& t ) {
    bs << std::get< I >( t );
    writeTuple< B, I + 1, Tp... >( bs, t );
}

template< typename B, std::size_t I, typename... Tp >
inline typename std::enable_if< (I == sizeof...(Tp)), void >::type
readTuple( base< B > &, std::tuple< Tp... >& ) {}

template< typename B, std::size_t I, typename... Tp >
inline typename std::enable_if< (I < sizeof...(Tp)), void >::type
readTuple( base< B > &bs, std::tuple< Tp... >& t ) {
    bs >> std::get< I >( t );
    readTuple< B, I + 1, Tp... >( bs, t );
}

template< typename B, typename... Tp >
inline base< B > &operator<<( base< B > &bs, std::tuple< Tp... >& t  ) {
    writeTuple< B, 0 >( bs, t );
    return bs;
}

template< typename B, typename... Tp >
inline base< B > &operator>>( base< B > &bs, std::tuple< Tp... >& t  ) {
    readTuple< B, 0 >( bs, t );
    return bs;
}

template< typename B >
base< B > &operator<<( base< B > &bs, wibble::Unit ) {
    return bs;
}

template< typename B >
base< B > &operator>>( base< B > &bs, wibble::Unit ) {
    return bs;
}

template< typename B, typename T >
typename integer< B, T >::stream &operator>>( base< B > &bs, T &x ) {
    assert( bs.size() );
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
    size_t size;
    bs >> size;
    for ( int i = 0; i < int( size ); ++ i ) {
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
    int size = 0;
    bs >> size;

    if ( !size ) {
        blob = Blob();
        return bs;
    }

    assert( bs.pool );
    blob = bs.pool->allocate( size );

    assert( bs.pool );
    assert_leq( bs.pool->size( blob ), bs.size() * 4 );

    char *begin = bs.pool->dereference( blob );
    const char *end = begin + bs.pool->size( blob );
    uint32_t *ptr = reinterpret_cast< uint32_t * >( begin );
    while ( ptr < reinterpret_cast< const uint32_t * >( end ) ) {
        bs >> *ptr;
        ++ptr;
    }

    return bs;
}

template< typename B >
base< B > &operator<<( base< B > &bs, Blob blob )
{
    assert( bs.pool );
    if ( !bs.pool->valid( blob ) )
        return bs << 0;

    const int size = bs.pool->size( blob );
    bs << size;
    const char *begin = bs.pool->dereference( blob );
    const char *end = begin + size;
    const uint32_t *ptr = reinterpret_cast< const uint32_t * >( begin );
    while ( ptr < reinterpret_cast< const uint32_t * >( end ) ) {
        bs << *ptr;
        ++ptr;
    }
    return bs;
}

}

typedef bitstream_impl::base< std::deque< uint32_t > > bitstream;
typedef bitstream_impl::base< bitstream_impl::block > bitblock;

}

#endif
