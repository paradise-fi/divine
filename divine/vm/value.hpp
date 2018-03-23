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
#include <iomanip>

namespace divine::vm::value
{

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

template< int _width, bool is_signed = false >
struct Int : Base
{
    static const int width = _width;
    using Raw = brick::bitlevel::bitvec< width >;
    using Cooked = Choose< is_signed, typename _signed< width >::T, Raw >;

    union {
        Raw _raw;
        Cooked _cooked;
        GenericPointer _pointer;
    };

    Raw _m;
    bool _ispointer:1;
    uint8_t _taints:5;


    void checkptr( Int o, Int &result )
    {
        if ( _ispointer && !o._ispointer && result._pointer.object() == _pointer.object() )
            result._ispointer = true;
        if ( !_ispointer && o._ispointer && result._pointer.object() == o._pointer.object() )
            result._ispointer = true;
    }

    Int arithmetic( Int o, Raw r )
    {
        Int result( r, (_m & o._m) == full< Raw >() ? full< Raw >() : 0, false );
        result.taints( taints() | o.taints() );
        checkptr( o, result );
        return result;
    }

    Int bitwise( Raw r, Raw m, Int o )
    {
        Int result( r, (_m & o._m) | m, false );
        result.taints( taints() | o.taints() );
        checkptr( o, result );
        return result;
    }

    Int< 1, false > compare( Int o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        res.taints( taints() | o.taints() );
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

    void taints( uint8_t set ) { _taints = set; }
    uint8_t taints() { return _taints; }

    Int() : _raw( 0 ), _m( 0 ), _ispointer( false ), _taints( 0 ) {}
    explicit Int( Cooked i ) : _cooked( i ), _m( full< Raw >() ), _ispointer( false ), _taints( 0 ) {}
    Int( Raw r, Raw m, bool ptr ) : _raw( r ), _m( m ), _ispointer( ptr ), _taints( 0 ) {}

    Int< width, true > make_signed()
    {
        Int< width, true > result( _raw, _m, _ispointer );
        result.taints( _taints );
        return result;
    }

    template< int w >
    typename std::enable_if< (width > w), Raw >::type signbit() { return Raw( 1 ) << ( w - 1 ); }
    template< int w >
    typename std::enable_if< (width < w), Raw >::type signbit() { return 0; }

    template< int w > Int( Int< w, is_signed > i )
        : _cooked( i._cooked ), _m( i._m ), _ispointer( i._ispointer ), _taints( i._taints )
    {
        if ( width > w && ( !is_signed || ( _m & signbit< w >() ) ) )
            _m |= ( bitlevel::ones< Raw >( width ) & ~bitlevel::ones< Raw >( w ) );
        if ( width < _VM_PB_Full )
            _ispointer = false;
        if ( is_signed && w == 1 ) /* TODO cover other bitwidths? */
            _cooked = i._cooked ? -1 : 0;
    }

    template< typename T > Int( Float< T > f ) :
        _cooked( f.cooked() ), _m( f.defined() ? full< Raw >() : 0 ), _ispointer( false ),
        _taints( f.taints() )
    {
        using FC = typename Float< T >::Cooked;
        if ( f.cooked() > FC( std::numeric_limits< Cooked >::max() )
            || f.cooked() < FC( std::numeric_limits< Cooked >::min() ) )
            _m = 0;
    }

    Int operator|( Int o ) { return bitwise( _raw | o._raw, (_m &  _raw) | (o._m &  o._raw), o ); }
    Int operator&( Int o ) { return bitwise( _raw & o._raw, (_m & ~_raw) | (o._m & ~o._raw), o ); }
    Int operator^( Int o ) { return bitwise( _raw ^ o._raw, 0, o ); }
    Int operator~() { Int r = *this; r._raw = ~_raw; return r; }

    template< int w >
    Int operator<<( Int< w, false > sh )
    {
        Int result( 0, 0, false );
        result.taints( _taints | sh.taints() );
        if ( !sh.defined() )
            return result;
        result._raw = raw() << sh.cooked();
        result._m = _m << sh.cooked() | bitlevel::ones< Raw >( sh.cooked() );
        return result;
    }

    template< int w >
    Int operator>>( Int< w, false > sh )
    {
        Int result( 0, 0, false );
        result.taints( _taints | sh.taints() );
        if ( !sh.defined() )
            return result;

        const int bits = 8 * sizeof( Raw );
        result._m = _m >> sh._raw;
        if ( !is_signed || _m >> ( bits - 1 ) ) // unsigned or defined sign bit
        {
            if ( sh._raw >= bits ) /* all original bits rotated away */
                result._m = bitlevel::ones< Raw >( bits );
            else /* only some of the bits are new */
                result._m |= ~bitlevel::ones< Raw >( bits - sh._raw );
        }

        result._cooked = cooked() >> sh.cooked();
        return result;
    }

    friend std::ostream & operator<<( std::ostream &o, Int v )
    {
        std::stringstream def;
        if ( v._m == bitlevel::ones< Raw >( width ) )
            def << "d";
        else if ( v._m == 0 )
            def << "u";
        else
            def << std::hex << std::setw( width / 4 ) << std::setfill( '0' )
                << +( v._m & bitlevel::ones< Raw >( width ) );
        return o << "[i" << width << " " << brick::string::fmt( v.cooked() )
                 << " " << def.str() << ( v.taints() ? "t" : "" ) << "]";
    }
};

template< typename T >
struct Float : Base
{
    using Raw = brick::bitlevel::bitvec< sizeof( T ) * 8 >;
    using Cooked = T;
    static_assert( sizeof( T ) == sizeof( Cooked ) );

    union {
        Raw _raw;
        Cooked _cooked;
    };

    bool _defined:1;
    uint8_t _taints:5;

    Float() : Float( 0, false ) {}
    explicit Float( T t, bool def = true ) : _raw( 0 ), _defined( def ), _taints( 0 ) { _cooked = t; }

    template< int w, bool sig > Float( Int< w, sig > i )
        : _cooked( i.cooked() ), _defined( i.defined() ), _taints( i.taints() ) {}

    Float &operator=( const Float &o )
    {
        _cooked = o._cooked;
        _defined = o._defined;
        _taints = o._taints;
        return *this;
    }

    template< typename S > explicit Float( Float< S > o )
        : _cooked( o._cooked ), _defined( o._defined ), _taints( o._taints )
    {
        if ( sizeof( _cooked ) < sizeof( o._cooked ) &&
             std::isinf( _cooked ) && !std::isinf( o._cooked ) )
            _defined = false;
    }

    Raw defbits() { return _defined ? full< Raw >() : 0; }
    void defbits( Raw r ) { _defined = ( r == full< Raw >() ); }
    Raw raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }

    bool defined() { return _defined; }
    void defined( bool d ) { _defined = d; }
    bool pointer() { return false; }
    void pointer( bool ) {} /* ignore */

    void taints( uint8_t set ) { _taints = set; }
    uint8_t taints() { return _taints; }

    bool isnan() { return std::isnan( _cooked ); }
    T cooked() { return _cooked; }
    Int< 1, false > compare( Float o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        res.taints( taints() | o.taints() );
        return res;
    }

    Float make_signed() { return *this; }
    Float arithmetic( Float o, Cooked r )
    {
        Float result( r, defined() && o.defined() );
        result.taints( taints() | o.taints() );
        return result;
    }

    friend std::ostream & operator<<( std::ostream &o, Float v )
    {
        return o << "[f" << sizeof( Cooked ) * 8 << " " << v.cooked() << " "
                 << ( v.defined() ? 'd' : 'u' ) << ( v.taints() ? "t" : "" ) << "]";
    }
};

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
    bool _ispointer:1;
    uint8_t _taints:5;

    template< typename P >
    auto offset( GenericPointer p ) { return P( p ).offset(); }

    template< typename L >
    auto withType( L l )
    {
        if ( _cooked.type() == PointerType::Global )
            return GenericPointer( l( GlobalPointer( _cooked ) ) );
        if ( _cooked.heap() )
            return GenericPointer( l( HeapPointer( _cooked ) ) );
        if ( _cooked.type() == PointerType::Code )
            return GenericPointer( l( CodePointer( _cooked ) ) );
        UNREACHABLE( "impossible pointer type" );
    }

    friend std::ostream &operator<<( std::ostream &o, Pointer v )
    {
        std::string def = "ddp";
        if ( !v._obj_defined ) def[0] = 'u';
        if ( !v._off_defined ) def[1] = 'u';
        if ( !v._ispointer ) def[2] = 'n';
        v.withType( [&]( auto p ) { o << "[" << p << " " << def << ( v.taints() ? "t" : "" )
                                      << "]"; return p; } );
        return o;
    }

    Pointer() : Pointer( nullPointer(), false, false ) {}
    explicit Pointer( Cooked x, bool d = true, bool isptr = true )
        : _cooked( x ), _obj_defined( d ), _off_defined( d ), _ispointer( isptr ), _taints( 0 ) {}

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
        r.taints( taints() | off.taints() );
        return r;
    }

    Raw defbits() { return defined() ? full< Raw >() : 0; } /* FIXME */
    void defbits( Raw r ) { _obj_defined = _off_defined = ( r == full< Raw >() ); }

    Raw raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }

    bool defined() { return _obj_defined && _off_defined; }
    void defined( bool d ) { _obj_defined = _off_defined = d; }

    void taints( uint8_t set ) { _taints = set; }
    uint8_t taints() { return _taints; }

    bool pointer() { return _ispointer; }
    void pointer( bool b ) { _ispointer = b; }

    GenericPointer cooked() { return _cooked; }
    void v( GenericPointer p ) { _cooked = p; }

    Int< 1, false > compare( Pointer o, bool v )
    {
        Int< 1, false > res( v );
        res.defined( defined() && o.defined() );
        res.taints( taints() | o.taints() );
        return res;
    }

    template< int w, bool s > operator Int< w, s >()
    {
        using IntPtr = Int< _VM_PB_Full, false >;
        IntPtr result( _cooked.raw(), defbits(), true );
        result.taints( taints() );
        return result;
    }

    template< int w, bool s > explicit Pointer( Int< w, s > i )
        : _cooked( PointerType::Global ), _taints( i.taints() )
    {
        if ( w >= PointerBytes )
        {
            _cooked.raw( i.raw() );
            _obj_defined = _off_defined = i.defined();
            _ispointer = i.pointer();
        }
        else /* truncated pointers are undef */
        {
            _cooked = nullPointer();
            _obj_defined = _off_defined = false;
            _ispointer = false;
        }
    }

    template< typename T > explicit Pointer( Float< T > )
    {
        UNREACHABLE( "invalid conversion from a float to a pointer" );
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

namespace divine::vm
{

static inline value::Pointer nullPointerV() { return value::Pointer( nullPointer() ); }
using CharV = value::Int< 8, true >;
using IntV = value::Int< 32, true >;
using PointerV = value::Pointer;
using BoolV = value::Bool;

}

namespace divine::t_vm
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
        ASSERT_EQ( res1._m, 0xC3FF );

        Int16 e( 1, 0xFEFF, false );
        ASSERT( !( c >> e ).defined() );
    }
};

struct TestPtr
{
    TEST( defined )
    {
        vm::value::Pointer p( vm::nullPointer(), true );
        ASSERT( p.defined() );
        p.defbits( -1 );
        ASSERT( p.defined() );
        p.defbits( 32 );
        ASSERT( !p.defined() );
    }

    TEST( isptr )
    {
        vm::value::Pointer p( vm::HeapPointer( 3, 0 ), true, true );
        ASSERT( p.pointer() );
        p = p + 20;
        ASSERT( p.pointer() );
        ASSERT_EQ( p.cooked().offset(), 20 );
        p = p + (-20);
        ASSERT( p.pointer() );
        ASSERT_EQ( p.cooked().object(), 3 );
        ASSERT_EQ( p.cooked().offset(), 0 );
    }

    TEST( intptr )
    {
        using I = vm::value::Int< vm::PointerBytes * 8 >;
        vm::value::Pointer p( vm::HeapPointer( 3, 0 ), true, true );
        I i( p );
        i = i + I( 20 );
        p = vm::value::Pointer( i );
        ASSERT( p.pointer() );
        ASSERT_EQ( p.cooked().object(), 3 );
        ASSERT_EQ( p.cooked().offset(), 20 );
    }

};

struct TestTaint
{
    TEST( def )
    {
        vm::value::Pointer p( vm::nullPointer(), true );
        vm::value::Int< 32 > i;
        ASSERT_EQ( p.taints(), 0 );
        ASSERT_EQ( i.taints(), 0 );
    }

    TEST( arith )
    {
        vm::value::Int< 32 > a( 7 ), b( 8 );
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        auto c = a + b;
        ASSERT_EQ( c.taints(), 1 );
        c = c + c;
        ASSERT_EQ( c.taints(), 1 );
        c = b + b;
        ASSERT_EQ( c.taints(), 0 );
        c = a * a;
        ASSERT_EQ( c.taints(), 1 );
        c = a * b;
        ASSERT_EQ( c.taints(), 1 );
        c = b * b;
        ASSERT_EQ( c.taints(), 0 );
    }

    TEST( multiple )
    {
        vm::value::Int< 32 > a( 7 ), b( 8 );
        a.taints( 1 );
        b.taints( 2 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 2 );
        auto c = a + b;
        ASSERT_EQ( c.taints(), 3 );
        c = c + c;
        ASSERT_EQ( c.taints(), 3 );
        c = a + a;
        ASSERT_EQ( c.taints(), 1 );
        c = a * a;
        ASSERT_EQ( c.taints(), 1 );
        c = b * b;
        ASSERT_EQ( c.taints(), 2 );
        c = a * b;
        ASSERT_EQ( c.taints(), 3 );
    }

    TEST( bitwise )
    {
        vm::value::Int< 32 > a( 7 ), b( 8 );
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        auto c = a & b;
        ASSERT_EQ( c.taints(), 1 );
        c = a | b;
        ASSERT_EQ( c.taints(), 1 );
        c = a ^ b;
        ASSERT_EQ( c.taints(), 1 );
        c = b;
        ASSERT_EQ( c.taints(), 0 );
    }

    TEST( ptr )
    {
        vm::value::Pointer a, b;
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        a = b;
        ASSERT_EQ( a.taints(), 0 );
        a.taints( 3 );
        ASSERT_EQ( a.taints(), 3 );
        vm::value::Int< 64 > x = a;
        ASSERT_EQ( x.taints(), 3 );
        b = vm::value::Pointer( x );
        ASSERT_EQ( b.taints(), 3 );
    }

    TEST( flt )
    {
        vm::value::Float< double > a, b;
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        auto c = a + b;
        ASSERT_EQ( c.taints(), 1 );
        c = a * b;
        ASSERT_EQ( c.taints(), 1 );
        c = c * c;
        ASSERT_EQ( c.taints(), 1 );
        c = c + a;
        ASSERT_EQ( c.taints(), 1 );
        c = c + b;
        ASSERT_EQ( c.taints(), 1 );
        c = b + b;
        ASSERT_EQ( c.taints(), 0 );
    }

    TEST( conv_1 )
    {
        vm::value::Float< double > a;
        a.taints( 1 );
        auto b = vm::value::Int< 64 >( a );
        ASSERT_EQ( b.taints(), 1 );
        auto p = vm::value::Pointer( b );
        ASSERT_EQ( p.taints(), 1 );
        auto i = vm::value::Int< 64 >( p );
        ASSERT_EQ( i.taints(), 1 );
        auto f = vm::value::Float< double >( i );
        ASSERT_EQ( f.taints(), 1 );
    }

    TEST( conv_0 )
    {
        vm::value::Float< double > a;
        auto b = vm::value::Int< 64 >( a );
        ASSERT_EQ( b.taints(), 0 );
        auto p = vm::value::Pointer( b );
        ASSERT_EQ( p.taints(), 0 );
        auto i = vm::value::Int< 64 >( p );
        ASSERT_EQ( i.taints(), 0 );
        auto f = vm::value::Float< double >( i );
        ASSERT_EQ( f.taints(), 0 );
    }
};

}
