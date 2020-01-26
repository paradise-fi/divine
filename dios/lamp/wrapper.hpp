// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2020 Petr Ročkai <code@fixp.eu>
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

/* This file is supposed to be included, but is not really a header. Instead,
 * it implements the LART-facing metadomain wrapper. Each metadomain should be
 * compiled into a separate bitcode file which contains exactly one copy
 * (build) of this file. See e.g. dios/metadom/trivial.cpp for an example. */

#include <sys/lamp.h>
#include <dios/lava/base.hpp> /* iN */

using ptr = __lamp_ptr;
using dom = __lamp::meta_domain;
using bw = __lava::bitwidth_t;

using namespace __lava; /* iN */

struct wrapper
{
    template< typename op_t >
    __inline static ptr wrap( op_t op ) { return { op().disown() }; }

    template< typename op_t, typename... args_t >
    __inline static ptr wrap( op_t op, ptr arg, args_t... args )
    {
        dom a( arg.ptr, dom::construct_shared );
        auto rv = wrap( [&]( const auto & ... args ) __inline { return op( a, args... ); }, args... );
        a.disown();
        return rv;
    }

    template< typename op_t, typename arg_t, typename... args_t >
    __inline static auto wrap( op_t op, const arg_t &arg, args_t... args )
        -> std::enable_if_t< !std::is_same_v< arg_t, dom >, ptr >
    {
        return wrap( [&] ( const auto & ... args ) __inline { return op( arg, args... ); }, args... );
    }
};

template< typename... args_t >
__inline static auto wrap( const args_t & ... args ) { return wrapper::wrap( args... ); }

static uint32_t tainted = 0;

[[gnu::constructor]] void tainted_init()
{
    __vm_pointer_t ptr = __vm_pointer_split( &tainted );
    __vm_poke( _VM_ML_Taints, ptr.obj, ptr.off, 4, 0xF );
}

template< typename op_t, typename arg_t, typename... args_t >
static arg_t lift( op_t op, arg_t arg, args_t... args )
{
    __lart_stash( op( arg, args... ).disown() );
    if constexpr ( std::is_arithmetic_v< arg_t > )
        return arg + tainted;
    if constexpr ( std::is_pointer_v< arg_t > )
        return static_cast< char * >( arg ) + tainted;
}

template< typename dom, typename type >
static auto any()
{
    __lart_stash( dom::template any< type >().disown() );
    if constexpr ( std::is_pointer_v< type > )
        return reinterpret_cast< type >( uint64_t( tainted ) );
    else
        return static_cast< type >( tainted );
}

#define export __invisible __flatten
#define scalar export __annotate( lart.abstract.return.scalar )
#define array  export __annotate( lart.abstract.return.aggregate )

extern "C"
{
    scalar i1  __lamp_lift_i1 ( i1  v )     { return lift( dom::lift_i1,  v ); }
    scalar i8  __lamp_lift_i8 ( i8  v )     { return lift( dom::lift_i8,  v ); }
    scalar i16 __lamp_lift_i16( i16 v )     { return lift( dom::lift_i16, v ); }
    scalar i32 __lamp_lift_i32( i32 v )     { return lift( dom::lift_i32, v ); }
    scalar i64 __lamp_lift_i64( i64 v )     { return lift( dom::lift_i64, v ); }
    scalar f32 __lamp_lift_f32( f32 v )     { return lift( dom::lift_f64, v ); }
    scalar f64 __lamp_lift_f64( f64 v )     { return lift( dom::lift_f64, v ); }

    scalar ptr __lamp_wrap_i1 ( i1  v )     { return wrap( dom::lift_i1,  v ); }
    scalar ptr __lamp_wrap_i8 ( i8  v )     { return wrap( dom::lift_i8,  v ); }
    scalar ptr __lamp_wrap_i16( i16 v )     { return wrap( dom::lift_i16, v ); }
    scalar ptr __lamp_wrap_i32( i32 v )     { return wrap( dom::lift_i32, v ); }
    scalar ptr __lamp_wrap_i64( i64 v )     { return wrap( dom::lift_i64, v ); }
    scalar ptr __lamp_wrap_f32( f32 v )     { return wrap( dom::lift_f64, v ); }
    scalar ptr __lamp_wrap_f64( f64 v )     { return wrap( dom::lift_f64, v ); }

    export ptr   __lamp_wrap_ptr( void *v ) { return wrap( dom::lift_ptr, v ); }
    export void *__lamp_lift_ptr( void *v ) { return lift( dom::lift_ptr, v ); }

    array void *__lamp_lift_arr( void *v, int s ) { return lift( dom::lift_arr, v, s ); }
    array char *__lamp_lift_str( char *s ) { return lift( dom::lift_arr, s, strlen( s ) + 1 ); }

    scalar i8    __lamp_any_i8()    { return any< dom, i8  >(); }
    scalar i16   __lamp_any_i16()   { return any< dom, i16 >(); }
    scalar i32   __lamp_any_i32()   { return any< dom, i32 >(); }
    scalar i64   __lamp_any_i64()   { return any< dom, i64 >(); }
    array  char *__lamp_any_array() { return any< dom, char * >(); }

    /* TODO freeze, melt */
    export void __lamp_freeze( void *val, void *addr_ )
    {
        __vm_pointer_t addr = __vm_pointer_split( addr_ ),
                       ptr  = __vm_pointer_split( val );
        __vm_poke( _VM_ML_User, addr.obj, addr.off, 1, ptr.obj );
    }

    export void *__lamp_melt( void *addr_ )
    {
        __vm_pointer_t addr = __vm_pointer_split( addr_ );
        auto m = __vm_peek( _VM_ML_User, addr.obj, addr.off, 1 );
        return __vm_pointer_make( m.value, 0 );
    }

    export ptr __lamp_add ( ptr a, ptr b ) { return wrap( dom::op_add, a, b ); }
    export ptr __lamp_sub ( ptr a, ptr b ) { return wrap( dom::op_sub, a, b ); }
    export ptr __lamp_mul ( ptr a, ptr b ) { return wrap( dom::op_mul, a, b ); }
    export ptr __lamp_sdiv( ptr a, ptr b ) { return wrap( dom::op_sdiv, a, b ); }
    export ptr __lamp_udiv( ptr a, ptr b ) { return wrap( dom::op_udiv, a, b ); }
    export ptr __lamp_srem( ptr a, ptr b ) { return wrap( dom::op_srem, a, b ); }
    export ptr __lamp_urem( ptr a, ptr b ) { return wrap( dom::op_urem, a, b ); }

    export ptr __lamp_fadd( ptr a, ptr b ) { return wrap( dom::op_fadd, a, b ); }
    export ptr __lamp_fsub( ptr a, ptr b ) { return wrap( dom::op_fsub, a, b ); }
    export ptr __lamp_fmul( ptr a, ptr b ) { return wrap( dom::op_fmul, a, b ); }
    export ptr __lamp_fdiv( ptr a, ptr b ) { return wrap( dom::op_fdiv, a, b ); }
    export ptr __lamp_frem( ptr a, ptr b ) { return wrap( dom::op_frem, a, b ); }

    export ptr __lamp_shl ( ptr a, ptr b ) { return wrap( dom::op_shl,  a, b ); }
    export ptr __lamp_ashr( ptr a, ptr b ) { return wrap( dom::op_ashr, a, b ); }
    export ptr __lamp_lshr( ptr a, ptr b ) { return wrap( dom::op_lshr, a, b ); }
    export ptr __lamp_and ( ptr a, ptr b ) { return wrap( dom::op_and,  a, b ); }
    export ptr __lamp_or  ( ptr a, ptr b ) { return wrap( dom::op_or,   a, b ); }
    export ptr __lamp_xor ( ptr a, ptr b ) { return wrap( dom::op_xor,  a, b ); }

    export ptr __lamp_eq ( ptr a, ptr b )  { return wrap( dom::op_eq,  a, b ); }
    export ptr __lamp_ne ( ptr a, ptr b )  { return wrap( dom::op_ne,  a, b ); }
    export ptr __lamp_ugt( ptr a, ptr b )  { return wrap( dom::op_ugt, a, b ); }
    export ptr __lamp_uge( ptr a, ptr b )  { return wrap( dom::op_uge, a, b ); }
    export ptr __lamp_ult( ptr a, ptr b )  { return wrap( dom::op_ult, a, b ); }
    export ptr __lamp_ule( ptr a, ptr b )  { return wrap( dom::op_ule, a, b ); }
    export ptr __lamp_sgt( ptr a, ptr b )  { return wrap( dom::op_sgt, a, b ); }
    export ptr __lamp_sge( ptr a, ptr b )  { return wrap( dom::op_sge, a, b ); }
    export ptr __lamp_slt( ptr a, ptr b )  { return wrap( dom::op_slt, a, b ); }
    export ptr __lamp_sle( ptr a, ptr b )  { return wrap( dom::op_sle, a, b ); }

    export ptr __lamp_foeq( ptr a, ptr b ) { return wrap( dom::op_foeq, a, b ); }
    export ptr __lamp_fogt( ptr a, ptr b ) { return wrap( dom::op_fogt, a, b ); }
    export ptr __lamp_foge( ptr a, ptr b ) { return wrap( dom::op_foge, a, b ); }
    export ptr __lamp_folt( ptr a, ptr b ) { return wrap( dom::op_folt, a, b ); }
    export ptr __lamp_fole( ptr a, ptr b ) { return wrap( dom::op_fole, a, b ); }
    export ptr __lamp_ford( ptr a, ptr b ) { return wrap( dom::op_ford, a, b ); }
    export ptr __lamp_funo( ptr a, ptr b ) { return wrap( dom::op_funo, a, b ); }
    export ptr __lamp_fueq( ptr a, ptr b ) { return wrap( dom::op_fueq, a, b ); }
    export ptr __lamp_fugt( ptr a, ptr b ) { return wrap( dom::op_fugt, a, b ); }
    export ptr __lamp_fuge( ptr a, ptr b ) { return wrap( dom::op_fuge, a, b ); }
    export ptr __lamp_fult( ptr a, ptr b ) { return wrap( dom::op_fult, a, b ); }
    export ptr __lamp_fule( ptr a, ptr b ) { return wrap( dom::op_fule, a, b ); }

    export ptr __lamp_ffalse( ptr a, ptr b ) { return wrap( dom::op_ffalse, a, b ); }
    export ptr __lamp_ftrue ( ptr a, ptr b ) { return wrap( dom::op_ftrue,  a, b ); }

    export ptr __lamp_concat ( ptr a, ptr b ) { return wrap( dom::op_concat,  a, b ); }
    export ptr __lamp_trunc  ( ptr a, bw  b ) { return wrap( dom::op_trunc,   a, b ); }
    export ptr __lamp_fptrunc( ptr a, bw  b ) { return wrap( dom::op_fptrunc, a, b ); }
    export ptr __lamp_sitofp ( ptr a, bw  b ) { return wrap( dom::op_sitofp,  a, b ); }
    export ptr __lamp_uitofp ( ptr a, bw  b ) { return wrap( dom::op_uitofp,  a, b ); }
    export ptr __lamp_zext   ( ptr a, bw  b ) { return wrap( dom::op_zext,    a, b ); }
    export ptr __lamp_zfit   ( ptr a, bw  b ) { return wrap( dom::op_zfit,    a, b ); }
    export ptr __lamp_sext   ( ptr a, bw  b ) { return wrap( dom::op_sext,    a, b ); }
    export ptr __lamp_fpext  ( ptr a, bw  b ) { return wrap( dom::op_fpext,   a, b ); }
    export ptr __lamp_fptosi ( ptr a, bw  b ) { return wrap( dom::op_fptosi,  a, b ); }
    export ptr __lamp_fptoui ( ptr a, bw  b ) { return wrap( dom::op_fptoui,  a, b ); }

    export ptr __lamp_gep( ptr a, ptr b, uint64_t s )
    {
        return wrap( []( auto... x ) { return dom::op_gep( x ... ); }, a, b, s );
    }

    export ptr __lamp_store( ptr a, ptr b, bw w )
    {
        return wrap( []( auto... x ) { return dom::op_store( x ... ); }, a, b, w );
    }

    export ptr __lamp_load( ptr a, bw w ) { return wrap( dom::op_load, a, w ); }

    export uint8_t __lamp_to_tristate( ptr v )
    {
        dom a( v.ptr, dom::construct_shared );
        auto tri = dom::to_tristate( a );
        a.disown();
        return tri.value;
    }

    export ptr __lamp_assume( ptr a, ptr b, bool c ) { return wrap( dom::assume, a, b, c ); }
    export ptr __lamp_extract( ptr a, bw s, bw e ) { return wrap( dom::op_extract, a, s, e ); }
    export bool __lamp_decide( uint8_t tr ) { return bool( __lava::tristate( tr ) ); }

    scalar ptr __lamp_fn_strcmp( ptr a, ptr b ) { return wrap( dom::fn_strcmp, a, b ); }
    scalar ptr __lamp_fn_strcat( ptr a, ptr b ) { return wrap( dom::fn_strcat, a, b ); }
    scalar ptr __lamp_fn_strcpy( ptr a, ptr b ) { return wrap( dom::fn_strcpy, a, b ); }
    scalar ptr __lamp_fn_strlen( ptr a ) { return wrap( dom::fn_strlen, a ); }

    scalar ptr __lamp_fn_strchr( ptr a, ptr b )
    {
        return wrap( []( auto... x ) { return dom::fn_strchr( x... ); }, a, b );
    }

    scalar ptr __lamp_fn_memcpy( ptr a, ptr b, ptr c )
    {
        return wrap( []( auto... x ) { return dom::fn_memcpy( x... ); }, a, b, c );
    }
}
