// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <wibble/sfinae.h>
#include <divine/toolkit/blob.h>

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

/* Helpers. */

template< typename T >
T &as( char *mem ) {
    return *reinterpret_cast< T * >( mem );
}

template< typename T >
const T &as( const char *mem ) {
    return *reinterpret_cast< const T * >( mem );
}

template< typename T >
char *asmem( T &t ) {
    return reinterpret_cast< char * >( &t );
}

template< typename T >
const char *asmem( const T &t ) {
    return reinterpret_cast< const char * >( &t );
}

template< typename T >
struct LinearSize {
    template< typename U >
    static auto _get( wibble::Preferred, U &t ) -> decltype( t.offset( 0 ) ) {
        return t.offset( t.end() );
    }

    template< typename U >
    static auto _get( wibble::NotPreferred, U & ) -> int {
        return sizeof( T );
    }

    static auto get( T &t ) -> int
    {
        return _get( wibble::Preferred(), t );
    }
};

struct LinearAddress {
    Blob b;
    int offset;

    template< typename LinearOffset >
    LinearAddress( LinearAddress base, int index, LinearOffset offset )
        : b( base.b ), offset( base.offset + offset( index ) )
    {}

    LinearAddress( Blob b, int offset )
        : b( b ), offset( offset )
    {}

    LinearAddress() : offset( 0 ) {}

    char *dereference() {
        return b.data() + offset;
    }

    // TODO: Generalize to non-linear targets...
    template< typename LinearSize >
    void copy( LinearAddress to, LinearSize size ) {
        std::copy( dereference(), dereference() + size(), to.dereference() );
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
        T &instance = as< T >( base.dereference() );
        return _address< decltype( T::type( p ) ) >(
            Address( base, T::index( p ), [&]( int idx ) { return instance.offset( idx ); } ),
            ps... );
    }

    template< typename... Ps >
    auto get( Ps... ps ) -> typename TypeAt< Structure, Ps... >::T &
    {
        return as< typename TypeAt< Structure, Ps... >::T >( address( ps... ).dereference() );
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
    void copy( Addr to ) {
        to.copy( start, [&]() { return LinearSize< Structure >::get( get() ); } );
    }
};

/* A type-indexed tuple. */
template< int, typename... > struct _Tuple;

template< int Idx >
struct _Tuple< Idx >
{
    int _length() { return Idx; }
    int _offset( int i ) { assert_eq( i, Idx ); return 0; }
};

template< int _Idx, typename _Head, typename... _Tail >
struct _Tuple< _Idx, _Head, _Tail... > : _Tuple< _Idx + 1, _Tail... >
{
    static const int Idx = _Idx;

    typedef _Head Head;
    typedef _Tuple< _Idx + 1, _Tail... > Tail;

    int _offset( int i ) {
        if ( _Idx < i )
            return LinearSize< Head >::get( head() ) + tail()._offset( i );
        else
            return 0; /* found our index */
    }
    Tail &tail() { return as< Tail >( asmem( *this ) + LinearSize< Head >::get( head() ) ); }
    Head &head() { return as< Head >( asmem( *this ) ); }
};

template< typename... Ts >
class Tuple
{
    typedef _Tuple< 0, Ts... > Real;
    Real real;

    template< typename T, typename Tu >
    static auto _index() ->
        typename wibble::EnableIf< wibble::TSame< T, typename Tu::Head >, int >::T
    {
        return Tu::Idx;
    }

    template< typename T, typename Tu >
    static auto _index() ->
        typename wibble::DisableIf< wibble::TSame< T, typename Tu::Head >, int >::T
    {
        return _index< T, typename Tu::Tail >();
    }

    template< typename T >
    static int index( T ) {
        return _index< T, Real >();
    }
    int end() { return real._length(); }
    int offset( int idx ) { return real._offset( idx ); }

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
    int offset( int i ) { return sizeof( int ) + i * sizeof( T ); }
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

    T &atoffset( int off ) {
        return as< T >( asmem( *this ) + off );
    }

    T &at( int i ) {
        return self().atoffset( self().offset( i ) );
    }

    static T type( int ) { return T(); }
    static int index( int i ) { return i; }
    int end() { return _length; }
};

template< typename T >
class Array: _Array< T, Array< T > >
{
    int offset( int i, int base = sizeof( int ) ) {
        if ( !i )
            return base;
        int previous = offset( i - 1 );
        return previous + LinearSize< T >::get( this->atoffset( previous ) );
    }
    LENS_FRIEND
public:
    int &length() { return this->_length; }
};

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

}
}
