// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>
#include <wibble/test.h>

#include <divine/toolkit/bitstream.h>

#ifndef DIVINE_RPC_H
#define DIVINE_RPC_H

#define RPC_MAX 64

namespace divine {
namespace rpc {

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

template< typename F >
struct Param2 {};
template< typename _T, typename _R, typename P0, typename P1 >
struct Param2< _R (_T::*)( P0, P1 ) > { typedef P1 T; };

/* --------------------------------------------------------------------------
   ---- MARSHALLING
   -------------------------------------------------------------------------- */

template< typename T, typename F, int n >
int findID_helper( wibble::NotPreferred, F f );

template< typename F > int check( F f );
template< typename F > void check_void( F f );

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

template< typename T, typename X, template< typename, typename > class With, typename _F, typename BSI, typename BSO >
struct Apply {
    typedef _F F;
    typedef typename Return< F >::T R;
    typedef With< X, F > Wrapper;
    BSI &in;
    BSO &out;

    Apply( BSI &in, BSO &out ) : in( in ), out( out ) {}

    template< typename... P >
    auto grab( wibble::Preferred, P&&... p ) -> IS_VOID( decltype( With< X, F >()( p... ) ) )
    {
        With< X, F > f;
        f( p... );
    }

    template< typename... P >
    auto grab( wibble::Preferred, P&&... p ) -> NOT_VOID( decltype( With< X, F >()( p... ) ) )
    {
        With< X, F > f;
        out << f( p... );
    }

    template< typename... P >
    void grab( wibble::NotPreferred, P&&... )
    {
        assert_die(); // the function does not exist or overloads failed to resolve
    }

    template< typename FF >
    void match( X &&x, R (T::*f)() )
    {
        grab( wibble::Preferred(), std::forward< X >( x ), f );
    }

    template< typename FF >
    void match( X &&x, R (T::*f)( typename Param1< FF >::T ) )
    {
        typename Param1< FF >::T p1;
        in >> p1;

        grab( wibble::Preferred(), std::forward< X >( x ), f, p1 );
    }

    template< typename FF >
    void match( X &&x, R (T::*f)( typename Param1< FF >::T, typename Param2< FF >::T ) )
    {
        typename Param1< FF >::T p1;
        typename Param2< FF >::T p2;

        in >> p1 >> p2;
        grab( wibble::Preferred(), std::forward< X >( x ), f, p1, p2 );
    }
};

template< typename T, typename X, template< typename, typename > class With,
          int id, typename BSI, typename BSO >
auto applyID( wibble::Preferred, X &&x, BSI &in, BSO &out ) ->
    typename wibble::TPair< typename T::template RpcId< id, true >::Fun, void >::Second
{
    typedef typename T::template RpcId< id, true >::Fun F;
    F f = T::template RpcId< id, true >::fun();
    Apply< T, X, With, F, BSI, BSO > apply( in, out );
    apply.template match< F >( std::forward< X >( x ), f );
}

template< typename T, typename X, template< typename, typename > class With, int id, typename BSI, typename BSO >
void applyID( wibble::NotPreferred, X &&x, BSI &in, BSO &out ) {
    assert_die();
}

template< typename T, typename X, template< typename F, typename > class With, int n, typename BSI, typename BSO >
void lookupAndApplyID( int id, X &&x, BSI &in, BSO &out ) {
    assert( n );
    if ( n == id )
        applyID< T, X, With, n >( wibble::Preferred(), std::forward< X >( x ), in, out );
    else
        lookupAndApplyID< T, X, With, n == 0 ? 0 : (n - 1) >( id, std::forward< X >( x ), in, out );
}

template< typename T, template< typename, typename > class With, typename X, typename BSI, typename BSO >
void demarshallWith( X &&x, BSI &in, BSO &out )
{
    int id;
    in >> id;
    lookupAndApplyID< T, X, With, RPC_MAX >( id, std::forward< X >( x ), in, out );
}

template< typename T, typename BSI, typename BSO >
void demarshall( T t, BSI &in, BSO &out )
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
