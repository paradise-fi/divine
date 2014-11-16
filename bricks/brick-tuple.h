// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

/* This header provides functionality of unpacking tuples into function
 * arguments.
 *
 * It define 3 main functions:
 * pass( fn, tuple ) takes callable object and tuple and unpacks tuple
 * into arguments of fn
 *
 * pass( x, method, tuple ) which acts like previous version, except it
 * calls method pointer method on x (which can be either pointer or object
 * directly)
 *
 * passIx< ixs... >( ... ) which acts exactly like (one of) pass functions
 * above but forwards only values on specified indices (in order)
 *
 *
 * In all cases exact prototype of function which can be called depends on
 * type of tuple:
 * - if tuple is value or rvalue reference, none of the arguments in function
 *   shall be non-const lvalue references (that is, tuple constents are
 *   forwarded as rvalues)
 * - if tuple is lvalue reference, none of the arguments in function shall
 *   be rvalue references (that is, tuple contends are forwarded as lvalue)
 * - const qualification of tuple is preserved
 *
 * Furthermore, following functions and types are provided and not considered
 * implementation details:
 * template< int... > struct Indices; for holding indices of tuple
 *
 * template< typename Tuple > GetIndices; for obtaining indices of tuple
 * - defines single nested name Ix of form Indices< ix... > for some ix
 *   which represents indices of tuple (if Tuple is in form std::tuple< ... >)
 * - otherwise is empty type with no nested names
 *
 * bindThis( x, method )  produces callable object which calls x.*m( ... )
 *
 */

#include <tuple>
#include <type_traits>

#include <brick-assert.h>

#ifndef BRICK_TUPLE_H
#define BRICK_TUPLE_H

namespace brick {
namespace tuple {

template< int... Ixs >
struct Indices {
    static constexpr int size = sizeof...( Ixs );
};

template< int... > struct _Expand { };

template< int... is >
struct _Expand< 0, is... > {
    using Ix = Indices< 0, is... >;
};

template< int i, int... is >
struct _Expand< i, is... > {
    static_assert( i > 0, "invalid param in _Expand" );

    using Ix = typename _Expand< i - 1, i, is... >::Ix;
};

template< typename... > struct _GetIndices { };

template< typename T, typename... Ts >
struct _GetIndices< T, Ts... > {
    using Ix = typename _Expand< sizeof...( Ts ) >::Ix;
};

template<>
struct _GetIndices<> {
    using Ix = Indices<>;
};

template< typename > struct GetIndices;

template< typename... Args >
struct GetIndices< std::tuple< Args... > > : _GetIndices< Args... > { };

template< typename > struct _Pass { };

template< int... ix >
struct _Pass< Indices< ix... > > {

    template< typename Fn, typename Tuple >
    static auto call( Fn &&fn, typename std::remove_reference< Tuple >::type &tuple )
        -> decltype( fn( std::get< ix >( tuple )... ) )
    {
        return fn( std::get< ix >( tuple )... );
    }

    template< size_t i, typename Tuple >
    static auto getrvalue( Tuple &tuple )
        -> typename std::tuple_element< i, Tuple >::type &&
    {
        return static_cast< typename std::tuple_element< i, Tuple >::type && >(
                std::get< i >( tuple ) );
    }

    template< typename Fn, typename Tuple >
    static auto call( Fn &&fn, typename std::remove_reference< Tuple >::type &&tuple )
        -> decltype( fn( getrvalue< ix >( tuple )... ) )
    {
        return fn( getrvalue< ix >( tuple )... );
    }
};

template< typename T, typename Method >
class BindThis {
    T _self;
    Method _m;
  public:

    BindThis( T &&self, Method &&m ) :
        _self( std::forward< T >( self ) ),
        _m( std::forward< Method >( m ) )
    { }

    template< typename... Args >
    auto operator()( Args &&... args )
        -> decltype( (this->_self.*_m)( std::forward< Args >( args )... ) )
    {
        return (_self.*_m)( std::forward< Args >( args )... );
    }
};

template< typename T, typename Method >
auto bindThis( T &&self, Method &&method )
    -> typename std::enable_if< std::is_member_function_pointer< Method >::value,
                                BindThis< T, Method > >::type
{
    return BindThis< T, Method >(
            std::forward< T >( self ), std::forward< Method >( method ) );
}

template< typename T, typename Method >
auto bindThis( T *self, Method &&method )
    -> typename std::enable_if< std::is_member_function_pointer< Method >::value,
                                BindThis< T &, Method > >::type
{
    return BindThis< T &, Method >( *self, std::forward< Method >( method ) );
}

template< typename Fn, typename Tuple,
    typename Ix = typename GetIndices< typename std::decay< Tuple >::type >::Ix >
auto pass( Fn &&fn, Tuple &&tuple )
    -> decltype( _Pass< Ix >::template call< Fn, Tuple >(
                std::forward< Fn >( fn ), std::forward< Tuple >( tuple ) ) )
{
    return _Pass< Ix >::template call< Fn, Tuple >(
            std::forward< Fn >( fn ), std::forward< Tuple >( tuple ) );
}

template< typename T, typename Fn, typename Tuple,
    typename UIx = typename GetIndices< typename std::decay< Tuple >::type >::Ix >
auto pass( T &&obj, Fn &&method, Tuple &&tuple )
    -> decltype( pass( bindThis( std::forward< T >( obj ),
                                 std::forward< Fn >( method ) ),
                       std::forward< Tuple >( tuple ) ) )
{
    return pass( bindThis( std::forward< T >( obj ), std::forward< Fn >( method ) ),
                 std::forward< Tuple >( tuple ) );
}

template< int... ix, typename... Args >
auto passIx( Args &&...args )
    -> decltype( pass< Args..., Indices< ix... > >( std::forward< Args >( args )... ) )
{
    return pass< Args..., Indices< ix... > >( std::forward< Args >( args )... );
}

}
}

namespace brick_test {
namespace tuple {

using namespace brick::tuple;

struct Obj {
    void m1( int x ) { ASSERT_EQ( x, 42 ); }
    int m2( int x, int y ) { return x + y; }
    int m3( int x ) { return x + z; }

    int z;
};

struct Tuple {
    TEST(call_empty) {
        std::tuple<> t;
        bool called = false;
        pass( [&] { called = true; }, t );
        ASSERT( called );
    }

    TEST(call_single) {
        std::tuple< int > t( 42 );
        bool called = false;
        pass( [&]( int x ) {
                called = true;
                ASSERT_EQ( x, 42 );
            }, t );
        ASSERT( called );
    }

    TEST(call_multi) {
        bool called = false;
        std::tuple< int, bool &, bool * > t( 42, called, &called );
        pass( [&]( int x, bool &cr, bool *cp ) {
                called = true;
                ASSERT( cr );
                ASSERT( *cp );
                ASSERT( &cr == cp );
                ASSERT( &called == cp );
                ASSERT_EQ( x, 42 );
            }, t );
        ASSERT( called );
    }

    TEST(call_lvalue) {
        std::tuple< int > t( 0 );
        pass( []( int &x ) { x = 42; }, t );
        ASSERT_EQ( std::get< 0 >( t ), 42 );
        pass( []( const int &x ) { ASSERT_EQ( x, 42 ); }, t );
        // pass( []( int &&x ) { }, t ); // this should not compile
    }

    TEST(call_rvalue) {
        pass( []{ }, std::tuple<>() );
        pass( []( int x ) { ASSERT_EQ( x, 42 ); }, std::tuple< int >( 42 ) );
        pass( []( int &&x ) { ASSERT_EQ( x, 42 ); }, std::tuple< int >( 42 ) );
        // pass( []( int &x ) { ASSERT_EQ( x, 42 ); }, std::tuple< int >( 42 ) ); // this should not compile

        int x = 0;
        pass( []( int &x ) { x = 42; }, std::tuple< int & >( x ) );
        ASSERT_EQ( x, 42 );

        std::tuple< int, int > t( 40, 2 );
        x = pass( []( int x, int &&y ) { return x + y; }, std::move( t ) );
        ASSERT_EQ( x, 42 );
    }

    TEST(retval) {
        ASSERT_EQ( pass( []( int x ) { return x + 2; }, std::tuple< int >( 40 ) ), 42 );
        int z = pass( []( int x, int y ) { return x + y; }, std::tuple< int, int >( 40, 2 ) );
        ASSERT_EQ( z, 42 );
    }

    TEST(bind_this) {
        Obj o;
        o.z = 40;
        bindThis( o, &Obj::m1 )( 42 );
        ASSERT_EQ( bindThis( o, &Obj::m3 )( 2 ), 42 );
    }

    TEST(obj_bind) {
        Obj o;
        o.z = 40;
        pass( bindThis( o, &Obj::m1 ), std::tuple< int >( 42 ) );
        int r = pass( bindThis( o, &Obj::m2 ), std::tuple< int, int >( 40, 2 ) );
        ASSERT_EQ( r, 42 );
        r = pass( bindThis( o, &Obj::m3 ), std::tuple< int >( 2 ) );
        ASSERT_EQ( r, 42 );
        o.z = 2;
        r = pass( bindThis( o, &Obj::m3 ), std::tuple< int >( 40 ) );
        ASSERT_EQ( r, 42 );
    }

    TEST(obj) {
        Obj o;
        o.z = 40;
        pass( o, &Obj::m1, std::tuple< int >( 42 ) );
        int r = pass( o, &Obj::m2, std::tuple< int, int >( 40, 2 ) );
        ASSERT_EQ( r, 42 );
        r = pass( o, &Obj::m3, std::tuple< int >( 2 ) );
        ASSERT_EQ( r, 42 );
        o.z = 2;
        r = pass( o, &Obj::m3, std::tuple< int >( 40 ) );
        ASSERT_EQ( r, 42 );
    }

    TEST(objptr) {
        Obj o;
        o.z = 40;
        pass( &o, &Obj::m1, std::tuple< int >( 42 ) );
        int r = pass( &o, &Obj::m2, std::tuple< int, int >( 40, 2 ) );
        ASSERT_EQ( r, 42 );
        r = pass( &o, &Obj::m3, std::tuple< int >( 2 ) );
        ASSERT_EQ( r, 42 );
        o.z = 2;
        r = pass( &o, &Obj::m3, std::tuple< int >( 40 ) );
        ASSERT_EQ( r, 42 );
    }

    TEST(pass_ix) {
        std::tuple< int, long, bool, int, int > t( 42, 1024l * 1024l * 1024l * 1024l, true, 40, 2 );
        passIx< 0 >( []( int x ) { ASSERT_EQ( x, 42 ); }, t );
        passIx< 1 >( []( long x ) { ASSERT_EQ( x, 1024l * 1024l * 1024l * 1024l ); }, t );
        passIx< 2, 0 >( []( bool b, int x ) { ASSERT( b ); ASSERT_EQ( x, 42 ); }, t );
        int r = passIx< 3, 4 >( []( int x, int y ) { return x + y; }, t );
        ASSERT_EQ( r, 42 );
    }

    struct CheckConst {
        template< typename T >
        void operator()( T & ) {
            static_assert( std::is_const< T >::value, "const got lost" );
        }
    };

    TEST(call_const) {
        const std::tuple< int > t( 42 );
        pass( []( const int &x ) { ASSERT_EQ( x, 42 ); }, t );
        pass( []( int x ) { ASSERT_EQ( x, 42 ); }, t );
        pass( CheckConst(), t );

        std::tuple< const int > tc( 42 );
        pass( CheckConst(), tc );

        int x = 42;
        pass( CheckConst(), std::tuple< const int & >( x ) );
    }
};
}
}

#endif // BRICK_TUPLE_H
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
