// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <brick-mem>

namespace divine {

namespace vm {
namespace value {

struct Base { template< typename X > using Value = X; };
template< int, bool > struct Int;
using Bool = Int< 1, false >;

template< int i > struct _signed { using T = typename _signed< i + 1 >::T; };
template<> struct _signed<  8 > { using T = int8_t; };
template<> struct _signed< 16 > { using T = int16_t; };
template<> struct _signed< 32 > { using T = int32_t; };
template<> struct _signed< 64 > { using T = int64_t; };

template< bool l, typename A, typename B > struct _choose {};
template< typename A, typename B > struct _choose< true, A, B > { using T = A; };
template< typename A, typename B > struct _choose< false, A, B > { using T = B; };
template< bool l, typename A, typename B > using Choose = typename _choose< l, A, B >::T;
template< typename T > struct Float;

struct Void : Base
{
    using Raw = struct {};
    using Cooked = Raw;
    Void( Raw = Raw() ) {}
};

template< int width, bool is_signed = false >
struct Int : Base
{
    using Raw = brick::mem::bitvec< width >;
    using Cooked = Choose< is_signed, typename _signed< width >::T, Raw >;

    static const Raw _full = bitlevel::compiletime::fill( Raw( 1 ) << ( width - 1 ) );
    Raw _v, _m;
    bool _pointer:1;
    PointerType _pointer_type:7;

    Int arithmetic( Int o, Raw r )
    {
        if ( _pointer )
            NOT_IMPLEMENTED();
        return Int( r, (_m & o._m) == _full ? _full : 0 );
    }
    Int bitwise( Raw r, Raw m, Int o ) { return Int( r, (_m & o._m) | m, false ); }
    Int< 1, false > compare( Int o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        return res;
    }
    bool defined() { return _m == _full; }
    void defined( bool b ) { if ( b ) _m = _full; else _m = 0; }

    auto v() { return *reinterpret_cast< Cooked * >( &_v ); }
    auto raw() { return _v; }

    Int( int i = 0 ) : Int( i, _full ) {}
    Int( Raw v, Raw m, bool ptr = false ) : _v( v ), _m( m ), _pointer( ptr ) {}
    Int< width, true > make_signed() { return Int< width, true >( _v, _m ); }
    template< int w > Int( Int< w, is_signed > i )
        : _v( i._v ), _m( i._m ), _pointer( i._pointer ) {} // ?
    template< typename T > Int( Float< T > ) { NOT_IMPLEMENTED(); }

    Int operator|( Int o ) { return bitwise( _v | o._v, (_m &  _v) | (o._m &  o._v), o ); }
    Int operator&( Int o ) { return bitwise( _v & o._v, (_m & ~_v) | (o._m & ~o._v), o ); }
    Int operator^( Int o ) { return bitwise( _v ^ o._v, 0, o ); }
    Int operator<<( Int< width, false > sh ) { NOT_IMPLEMENTED(); }
    Int operator>>( Int< width, false > sh ) { NOT_IMPLEMENTED(); }

    friend std::ostream & operator<<( std::ostream &o, Int v )
    {
        return o << "[int " << brick::string::fmt( v.v() ) << "]";
    }
};

template< typename T >
struct Float : Base
{
    using Raw = T;
    using Cooked = Raw;
    Raw _v;
    bool _defined;

    Float( T t, bool def = true ) : _v( t ), _defined( def ) {}
    template< int w, bool sig > Float( Int< w, sig > ) { NOT_IMPLEMENTED(); }
    template< typename S > Float( Float< S > ) { NOT_IMPLEMENTED(); }
    bool defined() { return _defined; }
    bool isnan() { NOT_IMPLEMENTED(); }
    T v() { return _v; }
    Int< 1, false > compare( Float o, bool v ) { return Bool( v, o.defined() && defined() ); }
    Float make_signed() { return *this; }
    Float arithmetic( Float o, Raw r ) { return Float( r, defined() && o.defined() ); }
    friend std::ostream & operator<<( std::ostream &o, Float v ) { return o << v.v(); }
};

struct HeapPointerPlaceholder
{
    using Shadow = uint32_t;
    operator GenericPointer<>() { return GenericPointer<>( PointerType::Heap ); }
    HeapPointerPlaceholder( GenericPointer<> ) {}
    friend std::ostream & operator<<( std::ostream &o, HeapPointerPlaceholder v )
    {
        return o << "[placeholder]";
    }
};

template< typename HeapPointer = HeapPointerPlaceholder >
struct Pointer : Base
{
    using Raw = GenericPointer<>;
    using Shadow = typename HeapPointer::Shadow;
    using Cooked = Raw;
    Raw _v;
    Shadow _s;
    bool _obj_defined:1, _off_defined:1;

    template< typename P >
    auto offset( GenericPointer<> p ) { return P( p ).offset(); }

    template< typename L >
    auto withType( L l )
    {
        if ( _v.type() == PointerType::Const )
            return GenericPointer<>( l( ConstPointer( _v ) ) );
        if ( _v.type() == PointerType::Global )
            return GenericPointer<>( l( GlobalPointer( _v ) ) );
        if ( _v.type() == PointerType::Heap )
            return GenericPointer<>( l( HeapPointer( _v ) ) );
        if ( _v.type() == PointerType::Code )
            return GenericPointer<>( l( CodePointer( _v ) ) );
        UNREACHABLE( "impossible pointer type" );
    }

    friend std::ostream &operator<<( std::ostream &o, Pointer v )
    {
        v.withType( [&]( auto p ) { o << p; return p; } );
        return o;
    }

    Pointer( GenericPointer<> x = nullPointer(), bool d = true )
        : _v( x ), _s(), _obj_defined( d ), _off_defined( d ) {}

    Pointer operator+( int off )
    {
        Pointer r = *this;
        r._v = withType( [&off]( auto p ) { p.offset() += off; return p; } );
        return r;
    }

    template< int w > Pointer operator+( Int< w, true > off )
    {
        Pointer r = *this + off.v();
        if ( !off.defined() )
            r._off_defined = false;
        return r;
    }

    bool defined() { return _obj_defined && _off_defined; }
    GenericPointer<> v() { return _v; }
    void v( GenericPointer<> p ) { _v = p; }
    Int< 1, false > compare( Pointer o, bool v ) { return Bool( v, o.defined() && defined() ); }

    template< int w, bool s > operator Int< w, s >()
    {
        using IntPtr = Int< PointerBytes, false >;
        return IntPtr( *reinterpret_cast< IntPtr::Raw * >( _v._v.storage ),
                       defined() ? IntPtr::_full : 0 );
    }

    template< int w, bool s > Pointer( Int< w, s > i )
        : _v( PointerType::Const )
    {
        if ( w >= PointerBytes )
        {
            auto r = i.raw();
            char *bytes = reinterpret_cast< char * >( &r );
            std::copy( bytes, bytes + PointerBytes, _v._v.storage );
            _obj_defined = _off_defined = i.defined();
        }
        else /* truncated pointers are undef */
        {
            _v = nullPointer();
            _obj_defined = _off_defined = false;
        }
    }
};

template< typename T >
Float< T > operator%( Float< T > a, Float< T > b )
{
    return a.arithmetic( b, std::fmod( a.v(), b.v() ) );
}

#define OP(meth, op) template< typename T >                          \
    auto operator op( typename T::template Value< T > a, T b ) { return a.meth( b, a.v() op b.v() ); }

OP( arithmetic, + );
OP( arithmetic, - );
OP( arithmetic, * );
OP( arithmetic, / );
OP( arithmetic, % );

OP( compare, == );
OP( compare, != );
OP( compare, < );
OP( compare, <= );
OP( compare, > );
OP( compare, >= );

#undef OP

}
}

namespace t_vm
{

struct TestInt
{
    TEST( size )
    {
        ASSERT_EQ( sizeof( vm::value::Int< 16 >::Raw ), 2 );
        ASSERT_EQ( sizeof( vm::value::Int< 32 >::Raw ), 4 );
        ASSERT_EQ( sizeof( vm::value::Int< 64 >::Raw ), 8 );
        ASSERT_EQ( sizeof( vm::value::Int< 60 >::Raw ), 8 );
        ASSERT_EQ( sizeof( vm::value::Int< 10 >::Raw ), 2 );
    }

    TEST( arithmetic )
    {
        vm::value::Int< 16 > a( 0 ), b( 0 );
        vm::value::Bool x = ( a + b == a );
        ASSERT( x._v == 1 );
        ASSERT( x._m == 1 );
        a = 4;
        ASSERT( ( a != b ).v() );
        b = a;
        ASSERT( ( a == b ).v() );
        ASSERT( ( a == b ).defined() );
        b = vm::value::Int< 16 >( 4, 0 );
        ASSERT( !( a == b ).defined() );
        ASSERT( ( a == b ).v() );
    }

    TEST( bitwise )
    {
        vm::value::Int< 16 > a( 1, 0xFFFE ), b( 2, 0xFFFD );
        ASSERT( (a & b).v() == 0 );
        ASSERT( (a & b).defined() );
        ASSERT( !(a | b).defined() );
    }
};

}

}
