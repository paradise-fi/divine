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

#include <divine/vm/pointer.hpp>

#include <brick-mem>
#include <brick-bitlevel>

#include <cmath>

namespace divine {

namespace vm {
namespace value {

namespace bitlevel = brick::bitlevel;

struct Base { static const bool IsValue = true; static const bool IsPointer = false; };
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

template< typename T >
constexpr T full() { return bitlevel::compiletime::ones< T >( 8 * sizeof( T ) ); }

struct Void : Base
{
    using Raw = struct {};
    using Cooked = Raw;
    Void( Raw = Raw() ) {}
    friend std::ostream &operator<<( std::ostream &o, Void ) { return o << "[void]"; }

    Raw raw() { return Raw(); }
    void raw( Raw ) {}
};

template< int width, bool is_signed = false >
struct Int : Base
{
    using Raw = brick::bitlevel::bitvec< width >;
    using Cooked = Choose< is_signed, typename _signed< width >::T, Raw >;

    union {
        Raw _raw;
        Cooked _cooked;
        GenericPointer _pointer;
    };

    Raw _m;
    bool _ispointer:1;

    Int arithmetic( Int o, Raw r )
    {
        auto result = Int( r, (_m & o._m) == full< Raw >() ? full< Raw >() : 0, false );
        if ( _ispointer && !o._ispointer && result._pointer.object() == _pointer.object() )
            result._ispointer = true;
        return result;
    }
    Int bitwise( Raw r, Raw m, Int o ) { return Int( r, (_m & o._m) | m, false ); }
    Int< 1, false > compare( Int o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        return res;
    }

    Raw defbits() { return _m; }
    void defbits( Raw b ) { _m = b; }
    bool defined() { return defbits() == full< Raw >(); }
    void defined( bool d ) { defbits( d ? full< Raw >() : 0 ); }
    bool pointer() { return _ispointer; }
    void pointer( bool p ) { _ispointer = p; }
    auto raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }
    auto cooked() { return _cooked; }

    Int() : _raw( 0 ), _m( 0 ), _ispointer( false ) {}
    explicit Int( Cooked i ) : _cooked( i ), _m( full< Raw >() ), _ispointer( false ) {}
    Int( Raw r, Raw m, bool ptr ) : _raw( r ), _m( m ), _ispointer( ptr ) {}
    Int< width, true > make_signed() { return Int< width, true >( _raw, _m, _ispointer ); }
    template< int w > Int( Int< w, is_signed > i )
        : _raw( i._raw ), _m( i._m ), _ispointer( i._ispointer )
    {
        if ( width > w && ( !is_signed || ( _m & ( Raw( 1 ) << ( w - 1 ) ) ) ) )
            _m |= ( bitlevel::ones< Raw >( width ) & ~bitlevel::ones< Raw >( w ) );
        if ( width < PointerBits )
            _ispointer = false;
    }
    template< typename T > Int( Float< T > ) { NOT_IMPLEMENTED(); }

    Int operator|( Int o ) { return bitwise( _raw | o._raw, (_m &  _raw) | (o._m &  o._raw), o ); }
    Int operator&( Int o ) { return bitwise( _raw & o._raw, (_m & ~_raw) | (o._m & ~o._raw), o ); }
    Int operator^( Int o ) { return bitwise( _raw ^ o._raw, 0, o ); }
    Int operator~() { Int r = *this; r._raw = ~_raw; return r; }
    Int operator<<( Int< width, false > sh ) {
        if ( !sh.defined() )
            return Int( 0, 0, false );
        return Int( raw() << sh.cooked(),
                    _m << sh.cooked() | bitlevel::fill( 1 << sh.cooked() ) , false );
    }
    Int operator>>( Int< width, false > sh ) {
        if ( !sh.defined() )
            return Int( 0, 0, false );

        const int bits = 8 * sizeof( Raw );
        Raw mask = _m >> sh._raw;
        if ( !is_signed || _m >> ( bits - 1 ) ) // unsigned or defined sign bit
                mask |= ~bitlevel::fill( 1 << ( bits - sh._raw ) );

        return Int( cooked() >> sh.cooked(), mask, false );
    }

    friend std::ostream & operator<<( std::ostream &o, Int v )
    {
        return o << "[int " << brick::string::fmt( v.cooked() ) << "]";
    }
};

template< typename T >
struct Float : Base
{
    using Raw = brick::bitlevel::bitvec< sizeof( T ) * 8 >;
    using Cooked = T;

    union {
        Raw _raw;
        Cooked _cooked;
    };

    bool _defined;

    Float( T t = 0, bool def = true ) : _cooked( t ), _defined( def ) {}
    template< int w, bool sig > Float( Int< w, sig > ) { NOT_IMPLEMENTED(); }
    template< typename S > Float( Float< S > ) { NOT_IMPLEMENTED(); }

    Raw defbits() { return _defined ? full< Raw >() : 0; }
    void defbits( Raw r ) { _defined = ( r == full< Raw >() ); }
    Raw raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }

    bool defined() { return defbits() == full< Raw >(); }
    void defined( bool d ) { _defined = d; }
    bool pointer() { return false; }
    void pointer( bool ) {} /* ignore */

    bool isnan() { NOT_IMPLEMENTED(); }
    T cooked() { return _cooked; }
    Int< 1, false > compare( Float o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        return res;
    }
    Float make_signed() { return *this; }
    Float arithmetic( Float o, Cooked r ) { return Float( r, defined() && o.defined() ); }
    friend std::ostream & operator<<( std::ostream &o, Float v ) { return o << v.cooked(); }
};

struct HeapPointerPlaceholder
{
    operator GenericPointer() { return GenericPointer( PointerType::Heap ); }
    HeapPointerPlaceholder( GenericPointer ) {}
    friend std::ostream & operator<<( std::ostream &o, HeapPointerPlaceholder )
    {
        return o << "[placeholder]";
    }
};

template< typename HeapPointer = HeapPointerPlaceholder >
struct Pointer : Base
{
    static const bool IsPointer = true;
    using Cooked = GenericPointer;
    using Raw = brick::bitlevel::bitvec< sizeof( Cooked ) * 8 >;

    union {
        Raw _raw;
        Cooked _cooked;
    };

    bool _obj_defined:1, _off_defined:1;

    template< typename P >
    auto offset( GenericPointer p ) { return P( p ).offset(); }

    template< typename L >
    auto withType( L l )
    {
        if ( _cooked.type() == PointerType::Const )
            return GenericPointer( l( ConstPointer( _cooked ) ) );
        if ( _cooked.type() == PointerType::Global )
            return GenericPointer( l( GlobalPointer( _cooked ) ) );
        if ( _cooked.type() == PointerType::Heap )
            return GenericPointer( l( HeapPointer( _cooked ) ) );
        if ( _cooked.type() == PointerType::Code )
            return GenericPointer( l( CodePointer( _cooked ) ) );
        UNREACHABLE( "impossible pointer type" );
    }

    friend std::ostream &operator<<( std::ostream &o, Pointer v )
    {
        std::string def = "dd";
        if ( !v._obj_defined ) def[0] = 'u';
        if ( !v._off_defined ) def[1] = 'u';
        v.withType( [&]( auto p ) { o << "[pointer " << p << " " << def << "]"; return p; } );
        return o;
    }

    Pointer( Cooked x = nullPointer(), bool d = true )
        : _cooked( x ), _obj_defined( d ), _off_defined( d ) {}

    explicit Pointer( Raw x )
        : _raw( x ), _obj_defined( false ), _off_defined( false ) {}

    Pointer operator+( int off )
    {
        Pointer r = *this;
        r._cooked = withType( [&off]( auto p ) { p.offset( p.offset() + off ); return p; } );
        return r;
    }

    template< int w > Pointer operator+( Int< w, true > off )
    {
        Pointer r = *this + off.cooked();
        if ( !off.defined() )
            r._off_defined = false;
        return r;
    }

    Raw defbits() { return defined() ? full< Raw >() : 0; } /* FIXME */
    void defbits( Raw r ) { _obj_defined = _off_defined = ( r == full< Raw >() ); }
    Raw raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }

    bool defined() { return _obj_defined && _off_defined; }
    void defined( bool d ) { _obj_defined = _off_defined = d; }

    bool pointer() { return defined(); }
    void pointer( bool b ) { if ( !b ) defbits( 0 ); }
    GenericPointer cooked() { return _cooked; }
    void v( GenericPointer p ) { _cooked = p; }
    Int< 1, false > compare( Pointer o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        return res;
    }

    template< int w, bool s > operator Int< w, s >()
    {
        using IntPtr = Int< PointerBits, false >;
        return IntPtr( _cooked.raw(), defined(), true );
    }

    template< int w, bool s > explicit Pointer( Int< w, s > i )
        : _cooked( PointerType::Const )
    {
        if ( w >= PointerBytes )
        {
            _cooked.raw( i.raw() );
            _obj_defined = _off_defined = i.defined();
        }
        else /* truncated pointers are undef */
        {
            _cooked = nullPointer();
            _obj_defined = _off_defined = false;
        }
    }
};

template< typename T >
Float< T > operator%( Float< T > a, Float< T > b )
{
    return a.arithmetic( b, std::fmod( a.cooked(), b.cooked() ) );
}

#define OP(meth, op) template< typename T >                          \
    auto operator op(                                                \
        typename std::enable_if< T::IsValue, T >::type a, T b )     \
    { return a.meth( b, a.cooked() op b.cooked() ); }

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
    using Int16 = vm::value::Int< 16 >;

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
        Int16 a( 0 ), b( 0 );
        vm::value::Bool x = ( a + b == a );
        ASSERT( x._raw == 1 );
        ASSERT( x._m == 255 );
        a = Int16( 4 );
        ASSERT( ( a != b ).cooked() );
        b = a;
        ASSERT( ( a == b ).cooked() );
        ASSERT( ( a == b ).defined() );
        b = Int16( 4, 0, false );
        ASSERT( !( a == b ).defined() );
        ASSERT( ( a == b ).cooked() );
    }

    TEST( bitwise )
    {
        Int16 a( 1, 0xFFFE, false ), b( 2, 0xFFFD, false );
        ASSERT( (a & b).cooked() == 0 );
        ASSERT( (a & b).defined() );
        ASSERT( !(a | b).defined() );
    }

    TEST ( shift_right )
    {
        vm::value::Int< 16, true > a( 115, 0x7FFF, false );
        Int16 b( 2, 0xFFFF, false );
        auto res = a >> b;
        ASSERT_EQ( res.cooked(), 115 >> 2 );
        ASSERT_EQ( res._m, 0x1FFF );

        Int16 c( 1135, 0xFFF, false ), d( 2, 0xFFFF, false );
        auto res1 = c >> d;
        ASSERT_EQ( res1.cooked(), 1135 >> 2 );
        ASSERT_EQ( res1._m, 0x83FF );

        Int16 e( 1, 0xFEFF, false );
        ASSERT( !( c >> e ).defined() );
    }
};

struct TestPtr
{
    TEST( defined )
    {
        vm::value::Pointer<> p( vm::nullPointer(), true );
        ASSERT( p.defined() );
        p.defbits( -1 );
        ASSERT( p.defined() );
        p.defbits( 32 );
        ASSERT( !p.defined() );
    }
};

}

}
