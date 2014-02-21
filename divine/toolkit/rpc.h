// -*- C++ -*- (c) 2012-2013 Petr Rockai <me@mornfall.net>

#include <type_traits>
#include <wibble/test.h>
#include <divine/toolkit/list.h>
#include <divine/toolkit/bitstream.h>

#ifndef DIVINE_RPC_H
#define DIVINE_RPC_H

namespace divine {
namespace rpc {

template< typename Stream, typename F, typename... Args >
struct Marshall {
    Stream &s;
    F f;
    std::tuple< Args... > args;

    Marshall( Stream &s, F f, Args... args ) : s( s ), f( f ), args( args... ) {}

    template< typename L >
    void handle( L l ) {
        s << lookup( f, l ) << args;
    }
};

template< typename T, typename F >
struct Call {
    template< typename... Ps >
    auto operator()( T &t, F fun, Ps... ps ) -> decltype( (t.*fun)( ps... ) ) {
        return (t.*fun)( ps... );
    }
};

template<int ...> struct Indices {};

template<int N, int ...S>
struct IndicesTo: IndicesTo<N-1, N-1, S...> {};

template<int ...S>
struct IndicesTo<0, S...> {
  typedef Indices<S...> T;
};

template< template < typename, typename > class With, typename X, typename T, typename BSI, typename BSO >
struct DeMarshall {
    X &x;
    BSI &bsi;
    BSO &bso;

    DeMarshall( X &x, BSI &bsi, BSO &bso ) : x( x ), bsi( bsi ), bso( bso ) {}

    template< typename F, typename Args, int... indices >
    void invoke_with_list( list::not_preferred, F, Args, Indices< indices... > )
    {
        assert_unreachable( "demarshallWith failed to match" );
    }

    template< typename F, typename Args, int... indices >
    auto invoke_with_list( list::preferred, F f, Args args, Indices< indices... > )
        -> typename std::enable_if<
            std::is_void< decltype( With< X, F >()( x, f, list::decons< indices >( args )... ) )
                          >::value >::type
    {
        With< X, F >()(
            x, f, list::decons< indices >( args )... );
    }

    template< typename F, typename Args, int... indices >
    auto invoke_with_list( list::preferred, F f, Args args, Indices< indices... > )
        -> typename std::enable_if<
            !std::is_void< decltype( With< X, F >()( x, f, list::decons< indices >( args )... ) )
                           >::value >::type
    {
        bso << With< X, F >()(
            x, f, list::decons< indices >( args )... );
    }

    template< typename ToUnpack, typename F, typename Args >
    auto read_and_invoke( F f, Args args )
        -> typename std::enable_if< ToUnpack::length == 0 >::type
    {
        invoke_with_list( list::preferred(), f, args, typename IndicesTo< Args::length >::T() );
    }

    template< typename ToUnpack, typename F, typename Args >
    auto read_and_invoke( F f, Args args )
        -> typename std::enable_if< ToUnpack::length != 0 >::type
    {
        typename ToUnpack::Car car;
        bsi >> car;
        read_and_invoke< typename ToUnpack::Cdr >( f, cons( car, args ) );
    }

    template< typename T_, typename RV, typename... Args >
    void invoke( RV (T_::*f)( Args... ) ) {
        read_and_invoke< typename list::List< Args... >::T >( f, list::Nil() );
    }

    void handle( int id, list::Nil ) {
        assert_unreachable( "could not demarshall method %d", id );
    }

    template< typename L >
    auto handle( int id, L list )
        -> typename std::enable_if< L::length != 0 >::type
    {
        if ( id == 1 )
            invoke( list.car );
        else
            handle( id - 1, list.cdr );
    }

    template< typename L >
    void handle( L list ) {
        int id;
        bsi >> id;
        handle( id, list );
    }
};

struct Root {
    template< typename Req, typename L >
    static void rpc_request( Req r, L l ) {
        r.handle( l );
    }
};

template< typename BS, typename R, typename T, typename... Args >
void marshall( BS &bs, R (T::*f)( Args... ), Args... args ) {
    T::rpc_request( Marshall< BS, R (T::*)( Args... ), Args... >( bs, f, args... ) );
}

template< typename T, template< typename, typename > class With, typename X, typename BSI, typename BSO >
void demarshallWith( X &x, BSI &bsi, BSO &bso ) {
    T::rpc_request( DeMarshall< With, X, T, BSI, BSO >( x, bsi, bso ) );
}

template< typename BSI, typename BSO, typename T >
void demarshall( T &t, BSI &bsi, BSO &bso ) {
    demarshallWith< T, Call >( t, bsi, bso );
}

}
}

#define DIVINE_RPC(super, ...)                                       \
    template< typename Req, typename L = list::Nil >                  \
    static void rpc_request( Req req, L l = list::Nil() ) {           \
        super::rpc_request( req, concat( list::list( __VA_ARGS__ ), l ) );      \
    }

#endif
