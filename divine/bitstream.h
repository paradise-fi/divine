// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <vector>
#include <deque>
#include <string>

#ifndef DIVINE_BITSTREAM_H
#define DIVINE_BITSTREAM_H

namespace divine {
namespace bitstream_impl {

template< typename Bits >
struct base {
    Bits bits;
    void clear() { bits.clear(); }
    bool empty() { return bits.empty(); }
    size_t size() { return bits.size(); }
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

template< typename B, typename X >
typename container< B, X >::stream &operator<<( base< B > &bs, X x ) {
    bs << x.size();
    for ( typename X::const_iterator i = x.begin(); i != x.end(); ++i )
        bs << *i;
    return bs;
}

template< typename B, typename T >
typename integer< B, T >::stream &operator<<( base< B > &bs, T i ) {
    bs.bits.push_back( i );
    return bs;
}

template< typename B, typename T >
typename integer< B, T >::stream &operator>>( base< B > &bs, T &x ) {
    x = bs.bits.front();
    bs.bits.pop_front();
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

}

typedef bitstream_impl::base< std::deque< uint32_t > > bitstream;
typedef bitstream_impl::base< std::vector< uint32_t > > bitblock;

}

#endif
