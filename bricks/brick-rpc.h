// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * A brick for implementing remote procedure calls (mostly between multiple
 * instances of the same process image running on different machines). The
 * actual communication code is not included, only marshalling and
 * de-marshalling of requests.
 */

/*
 * (c) 2012-2013 Petr Ročkai <me@mornfall.net>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-unittest.h>
#include <brick-hlist.h>

#include <deque>
#include <type_traits>
#include <tuple> // for std::get

#ifndef BRICK_RPC_H
#define BRICK_RPC_H

namespace brick {
namespace rpc {

namespace _impl {

struct empty {};

template< typename Bits, typename Ext = empty >
struct base : Ext {
    typedef base< Bits, Ext > bitstream;
    Bits bits;

    void clear() { bits.clear(); }
    bool empty() { return bits.empty(); }
    int size() { return bits.size(); }
    void shift() { bits.pop_front(); }
    uint32_t &front() { return bits.front(); }
    void push( uint32_t i ) { bits.push_back( i ); }
    template< typename... X >
    base( X&&... x ) : Ext( std::forward< X >( x )... ) {}
};

struct block : std::vector< uint32_t > {};

template< typename Ext >
struct base< block, Ext > : Ext {
    typedef base< block, Ext > bitstream;
    block bits;
    int offset;

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

    template< typename... X >
    base( X&&... x ) : Ext( std::forward< X >( x )... ) {}
};

template< typename B, typename E >
struct sfinae {
    using stream = base< B, E >;
};

template< typename B, typename E, typename X >
struct container {};
template< typename B, typename E, typename T >
struct container< B, E, std::vector< T > > : sfinae< B, E > {};
template< typename B, typename E, typename T >
struct container< B, E, std::deque< T > > : sfinae< B, E > {};

template< typename B, typename E >
struct container< B, E, std::string > : sfinae< B, E > {};

template< typename B, typename E, typename X >
using integer = typename std::conditional< std::is_integral< X >::value &&
                                           sizeof( X ) <= 4, sfinae< B, E >, empty >::type;

template< typename B, typename E, typename X >
using int64 = typename std::conditional< std::is_integral< X >::value &&
                                         sizeof( X ) == 8, sfinae< B, E >, empty >::type;

template< typename B, typename E, typename X >
typename container< B, E, X >::stream &operator<<( base< B, E > &bs, X x ) {
    bs << x.size();
    for ( typename X::const_iterator i = x.begin(); i != x.end(); ++i )
        bs << *i;
    return bs;
}

template< typename B, typename E, typename T >
typename integer< B, E, T >::stream &operator<<( base< B, E > &bs, T i ) {
    bs.push( i );
    return bs;
}

template< typename B, typename E, typename T >
typename int64< B, E, T >::stream &operator<<( base< B, E > &bs, T i ) {
    union { uint64_t x64; struct { uint32_t a; uint32_t b; } x32; };
    x64 = i;
    bs << x32.a << x32.b;
    return bs;
}

template< typename B, typename E, typename T1, typename T2 >
base< B, E > &operator<<( base< B, E > &bs, std::pair< T1, T2 > i ) {
    return bs << i.first << i.second;
}

template< typename B, typename E, std::size_t I, typename... Tp >
inline typename std::enable_if< (I == sizeof...(Tp)), void >::type
writeTuple( base< B, E > &, std::tuple< Tp... >& ) {}

template< typename B, typename E, std::size_t I, typename... Tp >
inline typename std::enable_if< (I < sizeof...(Tp)), void >::type
writeTuple( base< B, E > &bs, std::tuple< Tp... >& t ) {
    bs << std::get< I >( t );
    writeTuple< B, E, I + 1, Tp... >( bs, t );
}

template< typename B, typename E, std::size_t I, typename... Tp >
inline typename std::enable_if< (I == sizeof...(Tp)), void >::type
readTuple( base< B, E > &, std::tuple< Tp... >& ) {}

template< typename B, typename E, std::size_t I, typename... Tp >
inline typename std::enable_if< (I < sizeof...(Tp)), void >::type
readTuple( base< B, E > &bs, std::tuple< Tp... >& t ) {
    bs >> std::get< I >( t );
    readTuple< B, E, I + 1, Tp... >( bs, t );
}

template< typename B, typename E, typename... Tp >
inline base< B, E > &operator<<( base< B, E > &bs, std::tuple< Tp... >& t  ) {
    writeTuple< B, E, 0 >( bs, t );
    return bs;
}

template< typename B, typename E, typename... Tp >
inline base< B, E > &operator>>( base< B, E > &bs, std::tuple< Tp... >& t  ) {
    readTuple< B, E, 0 >( bs, t );
    return bs;
}

template< typename B, typename E, typename T >
typename integer< B, E, T >::stream &operator>>( base< B, E > &bs, T &x ) {
    ASSERT( bs.size() );
    x = bs.front();
    bs.shift();
    return bs;
}

template< typename B, typename E, typename T >
typename int64< B, E, T >::stream &operator>>( base< B, E > &bs, T &i ) {
    union { uint64_t x64; struct { uint32_t a; uint32_t b; } x32; };
    bs >> x32.a >> x32.b;
    i = x64;
    return bs;
}

template< typename B, typename E, typename X >
typename container< B, E, X >::stream &operator>>( base< B, E > &bs, X &x ) {
    size_t size;
    bs >> size;
    for ( int i = 0; i < int( size ); ++ i ) {
        typename X::value_type tmp;
        bs >> tmp;
        x.push_back( tmp );
    }
    return bs;
}

template< typename B, typename E, typename T1, typename T2 >
base< B, E > &operator>>( base< B, E > &bs, std::pair< T1, T2 > &i ) {
    return bs >> i.first >> i.second;
}

}

typedef _impl::base< std::deque< uint32_t > > bitstream;
typedef _impl::base< _impl::block > bitblock;

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
    void invoke_with_list( hlist::not_preferred, F, Args, Indices< indices... > )
    {
        ASSERT_UNREACHABLE( "demarshallWith failed to match" );
    }

    template< typename F, typename Args, int... indices >
    auto invoke_with_list( hlist::preferred, F f, Args args, Indices< indices... > )
        -> typename std::enable_if<
            std::is_void< decltype( With< X, F >()( x, f, hlist::decons< indices >( args )... ) )
                          >::value >::type
    {
        static_cast< void >( args ); // if indices are {} we can get unused param warning
        With< X, F >()(
            x, f, hlist::decons< indices >( args )... );
    }

    template< typename F, typename Args, int... indices >
    auto invoke_with_list( hlist::preferred, F f, Args args, Indices< indices... > )
        -> typename std::enable_if<
            !std::is_void< decltype( With< X, F >()( x, f, hlist::decons< indices >( args )... ) )
                           >::value >::type
    {
        static_cast< void >( args );
        bso << With< X, F >()(
            x, f, hlist::decons< indices >( args )... );
    }

    template< typename ToUnpack, typename F, typename Args >
    auto read_and_invoke( F f, Args args )
        -> typename std::enable_if< ToUnpack::length == 0 >::type
    {
        invoke_with_list( hlist::preferred(), f, args, typename IndicesTo< Args::length >::T() );
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
        read_and_invoke< typename hlist::List< Args... >::T >( f, hlist::Nil() );
    }

    void handle( int id, hlist::Nil ) {
        ASSERT_UNREACHABLE( "could not demarshall method" ); // todo print id?
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

namespace brick_test {
namespace rpc {

using namespace ::brick::rpc;

struct Bitstream {
    TEST(_bitstream) {
        bitstream bs;
        bs << 1 << 2 << 3;
        int x;
        bs >> x; ASSERT_EQ( x, 1 );
        bs >> x; ASSERT_EQ( x, 2 );
        bs >> x; ASSERT_EQ( x, 3 );
        ASSERT( bs.empty() );
    }

    TEST(_bitblock) {
        bitblock bs;
        bs << 1 << 2 << 3;
        int x;
        bs >> x; ASSERT_EQ( x, 1 );
        bs >> x; ASSERT_EQ( x, 2 );
        bs >> x; ASSERT_EQ( x, 3 );
        ASSERT( bs.empty() );
    }

    TEST(_bitstream_64) {
        bitstream bs;
        bs << int64_t( 1 ) << int64_t( 2 ) << int64_t( 3 );
        uint64_t x;
        bs >> x; ASSERT_EQ( x, 1ull );
        bs >> x; ASSERT_EQ( x, 2ull );
        bs >> x; ASSERT_EQ( x, 3ull );
        ASSERT( bs.empty() );
    }
};

}
}

#define BRICK_RPC(super, ...)                                           \
    template< typename Req, typename L = ::brick::hlist::Nil >          \
    static void rpc_request( Req req, L l = ::brick::hlist::Nil() ) {             \
        super::rpc_request( req, concat( ::brick::hlist::list( __VA_ARGS__ ), l ) ); \
    }

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
