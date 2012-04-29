// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>
#include <wibble/test.h>
#include <deque>
#include <string>
#include <stdint.h>

#ifndef DIVINE_RPC_H
#define DIVINE_RPC_H

#define RPC_MAX 64

namespace divine {
namespace rpc {

struct bitstream {
    std::deque< uint32_t > bits;
    void clear() { bits.clear(); }
    bool empty() { return bits.empty(); }
};

template< typename X > struct bitstream_container {};
template< typename T > struct bitstream_container< std::vector< T > > {
    typedef bitstream stream; };
template< typename T > struct bitstream_container< std::deque< T > > {
    typedef bitstream stream; };
template<> struct bitstream_container< std::string > {
    typedef bitstream stream; };

template< typename X > struct bitstream_int {};
template<> struct bitstream_int< bool > { typedef bitstream stream; };
template<> struct bitstream_int< char > { typedef bitstream stream; };
template<> struct bitstream_int< int8_t > { typedef bitstream stream; };
template<> struct bitstream_int< int16_t > { typedef bitstream stream; };
template<> struct bitstream_int< int32_t > { typedef bitstream stream; };
template<> struct bitstream_int< uint8_t > { typedef bitstream stream; };
template<> struct bitstream_int< uint16_t > { typedef bitstream stream; };
template<> struct bitstream_int< uint32_t > { typedef bitstream stream; };

template< typename X >
typename bitstream_container< X >::stream &operator<<( bitstream &bs, X x ) {
    bs << x.size();
    for ( typename X::const_iterator i = x.begin(); i != x.end(); ++i )
        bs << *i;
    return bs;
}

template< typename T >
typename bitstream_int< T >::stream &operator<<( bitstream &bs, T i ) {
    bs.bits.push_back( i );
    return bs;
}

template< typename T >
typename bitstream_int< T >::stream &operator>>( bitstream &bs, T &x ) {
    x = bs.bits.front();
    bs.bits.pop_front();
    return bs;
}

template< typename X >
typename bitstream_container< X >::stream &operator>>( bitstream &bs, X &x ) {
    int size;
    bs >> size;
    for ( int i = 0; i < size; ++ i ) {
        typename X::value_type tmp;
        bs >> tmp;
        x.push_back( tmp );
    }
    return bs;
}

/* --------------------------------------------------------------------------
   ---- some helpers (maybe try to lift this from a lib somewhere?)
   -------------------------------------------------------------------------- */

template< typename F >
struct Owner {};
template< typename _T, typename R >
struct Owner< R (_T::*)() > { typedef _T T; };
template< typename _T, typename R, typename P0 >
struct Owner< R (_T::*)( P0 ) > { typedef _T T; };
template< typename _T, typename R, typename P0, typename P1 >
struct Owner< R (_T::*)( P0, P1 ) > { typedef _T T; };

template< typename R > struct Returnable { typedef R T; };
template<> struct Returnable< void > { typedef wibble::Unit T; };

template< typename F >
struct Return {};
template< typename _T, typename _R >
struct Return< _R (_T::*)() > { typedef _R T; typedef typename Returnable< _R >::T R; };
template< typename _T, typename _R, typename P0 >
struct Return< _R (_T::*)( P0 ) > { typedef _R T; typedef typename Returnable< _R >::T R; };
template< typename _T, typename _R, typename P0, typename P1 >
struct Return< _R (_T::*)( P0, P1 ) > { typedef _R T; typedef typename Returnable< _R >::T R; };

template< typename F >
struct Param1 {};
template< typename _T, typename _R, typename P0 >
struct Param1< _R (_T::*)( P0 ) > { typedef P0 T; };
template< typename _T, typename _R, typename P0, typename P1 >
struct Param1< _R (_T::*)( P0, P1 ) > { typedef P0 T; };

/* --------------------------------------------------------------------------
   ---- MARSHALLING
   -------------------------------------------------------------------------- */

template< typename T, typename F, int n >
int findID_helper( wibble::NotPreferred, F f );

template< typename F > int check( F f );

template< typename T, typename F, int n >
auto findID_helper( wibble::Preferred, F f ) ->
    decltype( check< typename T::template RpcId< n, true >::Fun >( f ) )
{
    assert( n );
    if ( f == T::template RpcId< n, true >::fun() )
        return n;
    return findID_helper< T, F, n == 0 ? 0 : (n - 1) >( wibble::Preferred(), f );
}

template< typename T, typename F, int n >
int findID_helper( wibble::NotPreferred, F f ) {
    assert( n );
    return findID_helper< T, F, n == 0 ? 0 : (n - 1) >( wibble::Preferred(), f );
}

template< typename F >
int findID( F f ) {
    return findID_helper< typename Owner< F >::T, F, RPC_MAX >( wibble::Preferred(), f );
}

template< typename T, typename R >
void marshall( R (T::*f)(), bitstream &s ) {
    s << findID_helper< T, R (T::*)(), RPC_MAX >( wibble::Preferred(), f );
}

template< typename T, typename R, typename P1 >
void marshall( R (T::*f)( P1 ), P1 p, bitstream &s ) {
    s << findID_helper< T, R (T::*)( P1 ), RPC_MAX >( wibble::Preferred(), f ) << p;
}

template< typename T, typename R, typename P1, typename P2 >
void marshall( R (T::*f)( P1 ), P1 p1, P2 p2, bitstream &s ) {
    s << findID_helper< T, R (T::*)( P1 ), RPC_MAX >( wibble::Preferred(), f ) << p1 << p2;
}

/* --------------------------------------------------------------------------
   ---- DE-MARSHALLING
   -------------------------------------------------------------------------- */

template< typename T, typename F >
struct Call {
    typename Return< F >::T operator()( T t, F fun ) { return (t.*fun)(); }

    template< typename P0 >
    typename Return< F >::T operator()( T t, F fun, P0 p0 ) { return (t.*fun)( p0 ); }
};

#define IS_VOID(x) typename wibble::EnableIf< wibble::TSame< x, void >, void >::T
#define NOT_VOID(x) typename wibble::DisableIf< wibble::TSame< x, void >, void >::T

template< typename F, typename... P >
auto catchReturn( wibble::Preferred, bitstream &out, F f, P&&... p ) -> IS_VOID( decltype( f( p... ) ) )
{
    f( p... );
}

template< typename F, typename... P >
auto catchReturn( wibble::Preferred, bitstream &out, F f, P&&... p ) -> NOT_VOID( decltype( f( p... ) ) )
{
    out << f( p... );
}

template< typename F, typename... P >
void catchReturn( wibble::NotPreferred, bitstream &, F, P&&... )
{
    assert_die(); // the function does not exist or overloads failed to resolve
}

template< typename T, typename X, template< typename, typename > class With, typename R >
void apply( X &&x, R (T::*f)(), bitstream &, bitstream &out )
{
    With< X, R (T::*)() > wrapper;
    catchReturn( wibble::Preferred(), out, wrapper, std::forward< X >( x ), f );
}

/* This could conceivably become a variadic template... */
template< typename T, typename X, template< typename, typename > class With, typename R, typename P1 >
void apply( X &&x, R (T::*f)( P1 ), bitstream &in, bitstream &out )
{
    P1 p;
    in >> p;
    With< X, R (T::*)( P1 ) > wrapper;
    catchReturn( wibble::Preferred(), out, wrapper, std::forward< X >( x ), f, p );
}

template< typename T, typename X, template< typename, typename > class With,
          typename R, typename P1, typename P2 >
void apply( X &&x, R (T::*f)( P1, P2 ), bitstream &in, bitstream &out )
{
    P1 p1; P2 p2;
    in >> p1 >> p2;
    With< X, R (T::*)( P1, P2 ) > wrapper;
    catchReturn( wibble::Preferred(), out, wrapper, std::forward< X >( x ), f, p1, p2 );
}

template< typename T, typename X, template< typename, typename > class With, int id >
typename wibble::TPair< typename T::template RpcId< id, true >::Fun, void >::Second
applyID( wibble::Preferred, X &&x, bitstream &in, bitstream &out ) {
    typedef typename T::template RpcId< id, true >::Fun F;
    F f = T::template RpcId< id, true >::fun();
    apply< T, X, With, typename Return< F >::T >( std::forward< X >( x ), f, in, out );
}

template< typename T, typename X, template< typename, typename > class With, int id >
void applyID( wibble::NotPreferred, X &&x, bitstream &in, bitstream &out ) {
    assert_die();
}

template< typename T, typename X, template< typename F, typename > class With, int n >
void lookupAndApplyID( int id, X &&x, bitstream &in, bitstream &out ) {
    assert( n );
    if ( n == id )
        applyID< T, X, With, n >( wibble::Preferred(), std::forward< X >( x ), in, out );
    else
        lookupAndApplyID< T, X, With, n == 0 ? 0 : (n - 1) >( id, std::forward< X >( x ), in, out );
}

template< typename T, template< typename, typename > class With, typename X >
void demarshallWith( X &&x, bitstream &in, bitstream &out )
{
    int id;
    in >> id;
    lookupAndApplyID< T, X, With, RPC_MAX >( id, std::forward< X >( x ), in, out );
}

template< typename T >
void demarshall( T t, bitstream &in, bitstream &out )
{
    demarshallWith< T, Call >( t, in, out );
}

template< typename T, typename F, int n >
F lookupID_helper( wibble::NotPreferred, int id );

template< typename T, typename F, int n >
typename wibble::EnableIf<
    wibble::TSame< typename T::template RpcId< n, true >::Fun, F >, F >::T
lookupID_helper( wibble::Preferred, int id ) {
    assert( n );
    if ( n == id )
        return T::template RpcId< n, true >::fun();
    return lookupID_helper< T, F, n == 0 ? 0 : n - 1 >( wibble::Preferred(), id );
}

template< typename T, typename F, int n >
F lookupID_helper( wibble::NotPreferred, int id ) {
    assert( n );
    return lookupID_helper< T, F, n == 0 ? 0 : n - 1 >( wibble::Preferred(), id );
}

template< typename F >
F lookupID( int id ) {
    return lookupID_helper< typename Owner< F >::T, F, RPC_MAX >( wibble::Preferred(), id );
}

#define RPC_CLASS template< int id, bool > struct RpcId;
#define RPC_ID(_type, _fun, _id) \
  template< bool xoxo > struct _type::RpcId< _id, xoxo > { \
    typedef decltype( &_type::_fun ) Fun; \
    static Fun fun() { return &_type::_fun; } \
  };


}
}

#endif
