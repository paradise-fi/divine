// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <brick-types.h>
#include <divine/toolkit/blob.h>

#ifndef DIVINE_TOOLKIT_LENS_H
#define DIVINE_TOOLKIT_LENS_H

/*
 * Lens provide a type-safe interface to acessing variable-sized objects,
 * stored in possibly disjoint address spaces. The addressing scheme for
 * sub-objects consists of 1) indexes and 2) offsets. Aggregate objects
 * (basically tuples and arrays) can be stored both in a linear and non-linear
 * fashion. Moreover, due to existence of arrays, the components need not have
 * a fixed size, making variable-size, type-safe layouts possible.
 */

namespace divine {
namespace lens {

template< typename T >
struct LinearSize {
    template< typename X > static int check( X );

    template< typename Addr, typename U >
    static auto _get( brick::types::Preferred, Addr a ) ->
        decltype( check( a.template as< U >().advance( a, 0 ) ) )
    {
        T &instance = a.template as< T >();
        return a.distance( instance.advance( a, instance.end() ) );
    }

    template< typename Addr, typename U >
    static auto _get( brick::types::NotPreferred, Addr ) -> int {
        return std::is_empty< T >::value ? 0 : sizeof( T );
    }

    template< typename Addr >
    static auto get( Addr a ) -> int
    {
        return _get< Addr, T >( brick::types::Preferred(), a );
    }
};

struct LinearAddress {
    Blob b;
    int offset;
    Pool* pool;

    LinearAddress( LinearAddress base, int /* index */, int offset )
        : b( base.b ), offset( base.offset + offset ), pool( base.pool )
    {}

    LinearAddress( Pool* pool, Blob b, int offset )
        : b( b ), offset( offset ), pool( pool )
    {}

    LinearAddress() : offset( 0 ), pool( nullptr )
    { }

    char *dereference() {
        ASSERT( pool != nullptr );
        ASSERT( pool->valid( b ) );
        return pool->dereference( b ) + offset;
    }

    template< typename T >
    T &as() {
        return *reinterpret_cast< T * >( dereference() );
    }

    int distance( LinearAddress b ) {
        return b.offset - offset;
    }

    // TODO: Generalize to non-linear targets...
    LinearAddress copy( LinearAddress to, int size ) {
        std::copy( dereference(), dereference() + size, to.dereference() );
        return LinearAddress( pool, to.b, to.offset + size );
    }

    void advance( int i ) {
        offset += i;
    }
};

#define LENS_FRIEND \
    template< typename __Addr, typename __T > friend struct Lens; \
    template< typename __S, typename... __T > friend struct TypeAt; \
    template< typename __T > friend struct LinearSize;

template< typename Structure, typename... Path > struct TypeAt;

template< typename Structure >
struct TypeAt< Structure >  {
    typedef Structure T;
};

template< typename Structure, typename P, typename... Path >
struct TypeAt< Structure, P, Path... > {
    typedef typename TypeAt< decltype( Structure::type( P() ) ), Path... >::T T;
};

/*
 * A lens projects an Address -- usually an untyped memory block or a generic
 * non-linear memory structure -- as Structure.
 */
template< typename _Address, typename Structure >
struct Lens
{
    typedef _Address Address;
    Address start;

    Lens( Address a ) : start( a ) {}

    template< typename >
    static Address _address( Address base ) { return base; }

    template< typename T, typename P, typename... Ps >
    static Address _address( Address base, P p, Ps... ps ) {
        T &instance = base.template as< T >();
        return _address< decltype( T::type( p ) ) >(
            instance.advance( base, T::index( p ) ), ps... );
    }

    template< typename... Ps >
    auto get( Ps... ps ) -> typename TypeAt< Structure, Ps... >::T &
    {
        return address( ps... ).template as< typename TypeAt< Structure, Ps... >::T >();
    }

    template< typename... Ps >
    auto address( Ps... ps ) -> Address
    {
        return _address< Structure >( start, ps... );
    }

    template< typename... Ps >
    auto sub( Ps... ps ) -> Lens< Address, typename TypeAt< Structure, Ps... >::T >
    {
        return Lens< Address, typename TypeAt< Structure, Ps... >::T >( address( ps... ) );
    }

    template< typename Addr >
    Addr copy( Addr to ) {
        return start.copy( to, LinearSize< Structure >::get( this->address() ) );
    }
};

/* A type-indexed tuple. */
template< int, typename... > struct _Tuple;

template< int Idx >
struct _Tuple< Idx >
{
    int _length() { return Idx; }
    template< typename Addr >
    static Addr _advance( Addr base, int i ) { ASSERT_EQ( i, Idx ); return base; }
};

template< int _Idx, typename _Head, typename... _Tail >
struct _Tuple< _Idx, _Head, _Tail... > : _Tuple< _Idx + 1, _Tail... >
{
    static const int Idx = _Idx;

    typedef _Head Head;
    typedef _Tuple< _Idx + 1, _Tail... > Tail;

    template< typename Addr >
    static Addr _advance( Addr base, int i ) {
        if ( _Idx < i )
            return Tail::_advance( Addr( base, 1, LinearSize< Head >::get( base ) ), i );
        else
            return base; /* found our index */
    }
};

template< typename... Ts >
class Tuple
{
    typedef _Tuple< 0, Ts... > Real;
    Real real;

    template< typename T, typename Tu >
    static auto _index() ->
        typename std::enable_if< std::is_same< T, typename Tu::Head >::value, int >::type
    {
        return Tu::Idx;
    }

    template< typename T, typename Tu >
    static auto _index() ->
        typename std::enable_if< !std::is_same< T, typename Tu::Head >::value, int >::type
    {
        return _index< T, typename Tu::Tail >();
    }

    template< typename T >
    static int index( T ) {
        return _index< T, Real >();
    }

    int end() { return real._length(); }

    template< typename Addr >
    Addr advance( Addr base, int idx ) { return Real::_advance( base, idx ); }

    template< typename T >
    static T type( T t ) {
        return t;
    }

    LENS_FRIEND
};

template< typename T >
class FixedArray
{
    int _length;
    static T type( int ) { return T(); }
    static int index( int i ) { return i; }

    int end() { return length(); }

    template< typename Addr >
    Addr advance( Addr base, int i ) { return Addr( base, i, sizeof( int ) + i * sizeof( T ) ); }

    LENS_FRIEND
public:
    int &length() { return _length; }
};

/*
 * TODO This could be made more efficient, maybe?
 */

template< typename T, typename Self >
struct _Array {
    int _length;

    Self &self() { return *static_cast< Self * >( this ); }

    static T type( int ) { return T(); }
    static int index( int i ) { return i; }
    int end() { return _length; }
};

template< typename T >
class Array: _Array< T, Array< T > >
{
    template< typename Addr >
    Addr advance( Addr a, int i, int base = sizeof( int ) ) {
        ASSERT_LEQ( 0, i );
        if ( !i )
            return Addr( a, -1, base );
        Addr previous = advance( a, i - 1, base );
        return Addr( previous, i, LinearSize< T >::get( previous ) );
    }
    LENS_FRIEND
public:
    int &length() { return this->_length; }
};

#if 0
/*
 * TODO This is not very useful yet, since the index is not built
 * automatically, nor updated. We need an update interface first.
 */
template< typename T >
class IndexedArray : _Array< T, IndexedArray< T > >
{
    int _offsets[0];
    int offset( int i ) {
        return sizeof( int ) * ( length() + 1 ) + _offsets[i];
    }
    LENS_FRIEND
public:
    int &length() { return this->_length; }
};
#endif

}
}

namespace divine_test {

using namespace divine::lens;

struct TestLens {

    divine::Pool pool;

    TEST(tuple) {
        divine::Blob blob = pool.allocate( 3 * sizeof( int ) + sizeof( float ) );
        pool.clear( blob );

        struct IntA { int i; IntA( int i = 0 ) : i( i ) {} };
        struct IntB { int i; IntB( int i = 0 ) : i( i ) {} };
        struct IntC { int i; IntC( int i = 0 ) : i( i ) {} };

        typedef Tuple< IntA, IntB, IntC, float > Foo;

        LinearAddress a( &pool, blob, 0 );
        Lens< LinearAddress, Foo > lens( a );

        lens.get( IntA() ) = 1;
        lens.get( IntB() ) = 2;
        lens.get( IntC() ) = 3;

        ASSERT_EQ( divine::fmtblob( pool, blob ), "[ 1, 2, 3, 0 ]" );
    }

    TEST(fixedArray) {
        divine::Blob blob = pool.allocate( sizeof( int ) * 10 );
        pool.clear( blob );

        typedef FixedArray< int > Array;

        LinearAddress a( &pool, blob, 0 );
        Lens< LinearAddress, Array > lens( a );

        lens.get().length() = 8;
        for ( int i = 0; i < 8; ++i )
            lens.get( i ) = i + 1;

        ASSERT_EQ( divine::fmtblob( pool, blob ), "[ 8, 1, 2, 3, 4, 5, 6, 7, 8, 0 ]" );
    }

    TEST(basic) {
        divine::Blob blob = pool.allocate( 200 );
        pool.clear( blob );

        struct IntArray1 : FixedArray< int > {};
        struct IntArray2 : FixedArray< int > {};
        struct Matrix : Array< FixedArray< int > > {};
        struct Witch : Tuple< IntArray1, IntArray2 > {};
        struct Doctor: Tuple< IntArray1, int, Witch, float, Matrix > {};

        LinearAddress a( &pool, blob, 0 );
        Lens< LinearAddress, Doctor > lens( a );

        lens.get( IntArray1() ).length() = 5;
        lens.get( Witch(), IntArray1() ).length() = 3;
        lens.get( Witch(), IntArray2() ).length() = 4;
        lens.get( float() ) = -1;

        lens.get( Matrix() ).length() = 4;
        lens.get( Matrix(), 0 ).length() = 4;
        lens.get( Matrix(), 1 ).length() = 4;
        lens.get( Matrix(), 2 ).length() = 4;
        lens.get( Matrix(), 3 ).length() = 4;

        for ( int i = 0; i < 4; ++i )
            for ( int j = 0; j < 4; ++j )
                lens.get( Matrix(), i, j ) = 100 + j + i * j;

        lens.get( int() ) = int( 365 );

        ASSERT_EQ( divine::fmtblob( pool, blob ),
                   "[ 5, 0, 0, 0, 0, 0, 365, 3, 0, 0, 0, 4, 0, 0, 0, 0,"
                   " 3212836864, 4, 4, 100, 101, 102, 103, 4, 100, 102,"
                   " 104, 106, 4, 100, 103, 106, 109, 4, 100, 104, 108,"
                   " 112, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]" );

        for ( int i = 0; i < lens.get( IntArray1() ).length(); ++ i )
            lens.get( IntArray1(), i ) = i + 10;
        for ( int i = 0; i < lens.get( Witch(), IntArray1() ).length(); ++ i )
            lens.get( Witch(), IntArray1(), i ) = i + 20;
        for ( int i = 0; i < lens.get( Witch(), IntArray2() ).length(); ++ i )
            lens.get( Witch(), IntArray2(), i ) = i + 30;


        Lens< LinearAddress, Witch > lens2 UNUSED = lens.sub( Witch() );

        ASSERT_EQ( lens.get( int() ), 365 );

        ASSERT_EQ( lens.get( IntArray1() ).length(), 5 );
        ASSERT_EQ( lens.get( IntArray1(), 0 ), 10 );
        ASSERT_EQ( lens2.get( IntArray1(), 1 ), 21 );
        ASSERT_EQ( lens.get( Witch(), IntArray2(), 0 ), 30 );

        ASSERT_EQ( divine::fmtblob( pool, blob ),
                   "[ 5, 10, 11, 12, 13, 14, 365, 3, 20, 21, 22, 4, 30, 31, 32, 33, 3212836864,"
                   " 4, 4, 100, 101, 102, 103, 4, 100, 102, 104, 106, 4, 100, 103, 106, 109, 4,"
                   " 100, 104, 108, 112, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]" );
    }

    TEST(copy) {
        int size = 2 * sizeof( int );
        divine::Blob blob = pool.allocate( size ), copy = pool.allocate( size );
        pool.clear( blob );
        pool.clear( copy );

        struct IntA { int i; IntA( int i = 0 ) : i( i ) {} };
        struct IntB { int i; IntB( int i = 0 ) : i( i ) {} };

        typedef Tuple< IntA, IntB > Foo;

        LinearAddress a( &pool, blob, 0 );
        LinearAddress b( &pool, copy, 0 );

        Lens< LinearAddress, Foo > lens( a );

        lens.get( IntA() ) = 1;
        lens.get( IntB() ) = 2;

        lens.sub( IntA() ).copy( b );
        ASSERT_EQ( divine::fmtblob( pool, copy ), "[ 1, 0 ]" );
        pool.clear( copy );

        lens.copy( b );
        ASSERT_EQ( divine::fmtblob( pool, copy ), "[ 1, 2 ]" );
    }

};

}

#endif
