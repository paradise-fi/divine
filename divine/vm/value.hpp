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

#include <divine/vm/types.hpp>
#include <divine/vm/pointer.hpp>

#include <brick-mem>
#include <brick-bitlevel>

#include <cmath>
#include <iomanip>

namespace divine::vm::value
{

namespace bitlevel = brick::bitlevel;
using bitlevel::bitcast;

struct Base
{
    static const bool IsValue = true;
    static const bool IsPointer = false;
    static const bool IsFix = true;
    void setup() {}
};

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

template< int _width, bool is_signed, bool is_dynamic >
struct Int : Base
{
    using Raw = brick::bitlevel::bitvec< _width >;
    using Cooked = Choose< is_signed, typename _signed< _width >::T, Raw >;

    Raw _raw, _m;

    static const uint8_t _isptr = _width > _VM_PB_Obj ? _width - _VM_PB_Obj : 0;
    static const uint8_t _notptr = _isptr + 1;

    struct FixInt {};
    struct DynInt { uint8_t width; };

    struct Meta : Choose< is_dynamic, DynInt, FixInt >
    {
        uint8_t pointer:( 1 + bitlevel::compiletime::MSB( _isptr ) );
        uint8_t taints:5;
        Meta() : pointer( _notptr ), taints( 0 ) {}
    } _meta;

    int width()
    {
        if constexpr ( is_dynamic )
        {
            ASSERT_LT( 0, _meta.width );
            return _meta.width;
        }
        else
            return _width;
    }

    int size()
    {
        if constexpr ( is_dynamic )
            return brick::bitlevel::align( width(), 8 ) / 8;
        else
            return sizeof( Raw );
    }

    uint32_t objid() { return objid( _meta.pointer ); }
    uint32_t objid( int offset )
    {
        if ( offset > _isptr )
            return 0; /* not a pointer */
        return ( _raw >> offset ) & brick::bitlevel::ones< Raw >( _VM_PB_Obj );
    }

    int objid_offset() { return _meta.pointer; }
    void objid_offset( int v ) { _meta.pointer = v; }

    void checkptr( Int o, Int &result, int shift = 0 )
    {
        if constexpr ( _width < _VM_PB_Obj )
            return;

        if ( objid() && result.objid( _meta.pointer + shift ) &&
             result.objid( _meta.pointer + shift ) == objid() )
            result._meta.pointer = _meta.pointer + shift;
        if ( o.objid() && result.objid( o._meta.pointer + shift )
                       && result.objid( o._meta.pointer + shift ) == o.objid() )
            result._meta.pointer = o._meta.pointer + shift;
    }

    Raw full() { return brick::bitlevel::ones< Raw >( width() ); }

    Int arithmetic( Int o, Raw r )
    {
        Int result( r, ( _m & o._m & full() ) == full() ? full() : 0, false );
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

    void write( Raw *r )
    {
        if constexpr ( is_dynamic )
        {
            char *rr = reinterpret_cast< char * >( &_raw );
            std::copy( rr, rr + size(), reinterpret_cast< char * >( r ) );
        }
        else
            *r = _raw;
    }

    void read( Raw *r )
    {
        if constexpr ( is_dynamic )
        {
            _raw = 0;
            char *rr = reinterpret_cast< char * >( r );
            std::copy( rr, rr + size(), reinterpret_cast< char * >( &_raw ) );
        }
        else
            _raw = *r;
    }

    Raw defbits() { return _m & full(); }
    void defbits( Raw b ) { _m = b; }
    bool defined() { return defbits() == full(); }
    void defined( bool d ) { defbits( d ? full() : 0 ); }
    bool pointer() { return _meta.pointer == _isptr; }
    void pointer( bool p ) { _meta.pointer = p ? _isptr : _notptr; }
    auto raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }
    auto cooked() { return bitcast< Cooked >( _raw ); }

    GenericPointer as_pointer()
    {
        ASSERT( pointer() );
        GenericPointer rv;
        bitlevel::maybe_bitcast( _raw, rv );
        return rv;
    }

    void taints( uint8_t set ) { _meta.taints = set; }
    uint8_t taints() { return _meta.taints; }

    Int() : _raw( 0 ), _m( 0 ) {}
    explicit Int( Cooked i ) : _raw( bitcast< Raw >( i ) ), _m( value::full< Raw >() ) {}
    Int( Raw r, Raw m, bool ptr ) : _raw( r ), _m( m )
    {
        if ( ptr ) _meta.pointer = _isptr;
    }

    Int< _width, true, is_dynamic > make_signed()
    {
        Int< _width, true, is_dynamic > result( _raw, _m, false );
        result._meta.pointer = _meta.pointer;
        result.taints( _meta.taints );
        return result;
    }

    template< int w > Raw signbit()
    {
        if constexpr ( _width >= w )
            return Raw( 1 ) << ( w - 1 );
        else
            return 0;
    }

    template< int w, bool dyn > Int( Int< w, is_signed, dyn > i )
        : _raw( i._raw ), _m( i._m )
    {
        ASSERT( !dyn ); /* FIXME */
        _meta.taints = i._meta.taints;

        if ( _width > w && ( !is_signed || ( _m & signbit< w >() ) ) )
            _m |= ( bitlevel::ones< Raw >( _width ) & ~bitlevel::ones< Raw >( w ) );

        if ( i._meta.pointer > _width - _VM_PB_Obj )
            _meta.pointer = _notptr;
        else
            _meta.pointer = i._meta.pointer;

        if ( is_signed )
            bitcast( Cooked( ( w == 1 && i.cooked() ) ? -1 : i.cooked() ), _raw );
    }

    template< typename T > Int( Float< T > f )
        : _raw( bitcast< Raw >( Cooked( f.cooked() ) ) ), _m( f.defined() ? value::full< Raw >() : 0 )
    {
        taints( f.taints() );
        using FC = typename Float< T >::Cooked;
        if ( f.cooked() > FC( std::numeric_limits< Cooked >::max() )
            || f.cooked() < FC( std::numeric_limits< Cooked >::min() ) )
            _m = 0;
    }

    Int operator|( Int o ) { return bitwise( _raw | o._raw, (_m &  _raw) | (o._m &  o._raw), o ); }
    Int operator&( Int o ) { return bitwise( _raw & o._raw, (_m & ~_raw) | (o._m & ~o._raw), o ); }
    Int operator^( Int o ) { return bitwise( _raw ^ o._raw, 0, o ); }
    Int operator~() { Int r = *this; r._raw = ~_raw; return r; }
    bool operator!() const { return !_raw; }

    template< int w, bool dyn >
    Int operator<<( Int< w, false, dyn > sh )
    {
        ASSERT( !dyn ); /* FIXME */
        Int result( 0, 0, false );
        result.taints( taints() | sh.taints() );
        if ( !sh.defined() )
            return result;
        result._raw = raw() << sh.cooked();
        result._m = _m << sh.cooked() | bitlevel::ones< Raw >( sh.cooked() );
        checkptr( Int(), result, sh.cooked() );
        return result;
    }

    template< int w, bool dyn >
    Int operator>>( Int< w, false, dyn > sh )
    {
        ASSERT( !dyn ); /* FIXME */
        Int result( 0, 0, false );
        result.taints( taints() | sh.taints() );
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

        bitcast( Cooked( cooked() >> sh.cooked() ), result._raw );
        checkptr( Int(), result, -sh.cooked() );
        return result;
    }

    friend std::ostream & operator<<( std::ostream &o, Int v )
    {
        std::stringstream def;
        auto aw = brick::bitlevel::align( _width, 8 );
        if ( v._m == bitlevel::ones< Raw >( aw ) )
            def << "d";
        else if ( v._m == 0 )
            def << "u";
        else
            def << std::hex << std::setw( aw / 4 ) << std::setfill( '0' )
                << +( v._m & bitlevel::ones< Raw >( aw ) );
        if ( v.pointer() ) def << "p";
        if ( v.taints() ) def << "t";

        return o << "[i" << v.width() << " " << brick::string::fmt( v.cooked() )
                 << " " << def.str() << "]";
    }
};

template< bool s >
struct DynInt : Int< 64, s, true >
{
    static const bool IsFix = false;
    using Super = Int< 64, s, true >;
    using Super::Super;

    DynInt( const Super &i ) : Super( i ) {}

    void setup( int bw )
    {
        ASSERT_LEQ( bw, 64 );
        this->_meta.width = bw;
    }

    DynInt< true > make_signed()
    {
        return Super::make_signed();
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

    int size() { return sizeof( Raw ); }
    void read( Raw *r ) { _raw = *r; }
    void write( Raw *r ) { *r = _raw; }

    Raw defbits() { return _defined ? full< Raw >() : 0; }
    void defbits( Raw r ) { _defined = ( r == full< Raw >() ); }
    Raw raw() { return _raw; }
    void raw( Raw r ) { _raw = r; }

    bool defined() { return _defined; }
    void defined( bool d ) { _defined = d; }
    bool pointer() { return false; }
    void pointer( bool ) {} /* ignore */

    uint32_t objid() { return 0; }
    int objid_offset() { return 0; }
    void objid_offset( int ) {}

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

    Cooked _cooked;
    bool _obj_defined:1, _off_defined:1;
    bool _ispointer:1;
    uint8_t _taints:5;

    int size() { return sizeof( Raw ); }
    void read( Raw *r ) { _cooked = bitcast< Cooked >( *r ); }
    void write( Raw *r ) { *r = bitcast< Raw >( _cooked ); }

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
        return l( _cooked );
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

    Raw raw() { return bitcast< Raw >( _cooked ); }
    void raw( Raw r ) { bitcast( r, _cooked ); }

    bool defined() { return _obj_defined && _off_defined; }
    void defined( bool d ) { _obj_defined = _off_defined = d; }

    void taints( uint8_t set ) { _taints = set; }
    uint8_t taints() { return _taints; }

    bool pointer() { return _ispointer; }
    void pointer( bool b ) { _ispointer = b; }

    GenericPointer cooked() { return _cooked; }
    void v( GenericPointer p ) { _cooked = p; }

    uint32_t objid() { return cooked().object(); }
    int objid_offset() { return 32; }
    void objid_offset( int ) {}

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
        : _taints( i.taints() )
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
