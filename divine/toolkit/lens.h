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

template< typename T >
struct LinearSize {
    template< typename X > static int check( X );

    template< typename Addr, typename U >
    static auto _get( wibble::Preferred, Addr a ) ->
        decltype( check( a.template as< U >().advance( a, 0 ) ) )
    {
        T &instance = a.template as< T >();
        return a.distance( instance.advance( a, instance.end() ) );
    }

    template< typename Addr, typename U >
    static auto _get( wibble::NotPreferred, Addr ) -> int {
        return sizeof( T );
    }

    template< typename Addr >
    static auto get( Addr a ) -> int
    {
        return _get< Addr, T >( wibble::Preferred(), a );
    }
};

struct LinearAddress {
    Blob b;
    int offset;

    LinearAddress( LinearAddress base, int index, int offset )
        : b( base.b ), offset( base.offset + offset )
    {}

    LinearAddress( Blob b, int offset )
        : b( b ), offset( offset )
    {}

    LinearAddress() : offset( 0 ) {}

    char *dereference() {
        return b.data() + offset;
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
        return LinearAddress( to.b, to.offset + size );
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
    static Addr _advance( Addr base, int i ) { assert_eq( i, Idx ); return base; }
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
