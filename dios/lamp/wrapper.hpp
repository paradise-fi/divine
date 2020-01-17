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

#include <sys/cdefs.h>
#include <rst/base.hpp>   /* iN */
#include <rst/common.hpp> /* bitwidth_t */

struct wrapped_ptr { void *ptr; };

using ptr = wrapped_ptr;
using dom = __rdom::meta_domain;
using bw = bitwidth_t;

template< typename op_t >
static ptr wrap( op_t op ) { return { op().disown() }; }

template< typename op_t, typename... args_t >
static ptr wrap( op_t op, ptr arg, args_t... args )
{
    dom a( arg.ptr, dom::construct_shared );
    auto rv = w( [&]( auto... args ) { return op( a, args... ); }, args... );
    a.disown();
    return rv;
}

template< typename op_t, typename arg_t, typename... args_t >
static auto wrap( op_t op, const arg_t &arg, args_t... args )
    -> std::enable_if_t< !std::is_same_v< arg_t, dom >, ptr >
{
    return w( [&] ( auto... args ) { return op( arg, args... ); }, args... );
}

#define export __invisible __flatten

extern "C"
{
    export ptr __mdom_lift_one_i1( i1 v )     { return wrap( dom::lift_one_i1,  v ); }
    export ptr __mdom_lift_one_i8( i8 v )     { return wrap( dom::lift_one_i8,  v ); }
    export ptr __mdom_lift_one_i16( i16 v )   { return wrap( dom::lift_one_i16, v ); }
    export ptr __mdom_lift_one_i32( i32 v )   { return wrap( dom::lift_one_i32, v ); }
    export ptr __mdom_lift_one_i64( i64 v )   { return wrap( dom::lift_one_i64, v ); }
    export ptr __mdom_lift_one_f32( f32 v )   { return wrap( dom::lift_one_f64, v ); }
    export ptr __mdom_lift_one_f64( f64 v )   { return wrap( dom::lift_one_f64, v ); }
    export ptr __mdom_lift_one_ptr( void *v ) { return wrap( dom::lift_one_ptr, v ); }

    /* TODO freeze, melt */

    export ptr __mdom_op_add ( ptr a, ptr b ) { return wrap( dom::op_add, a, b ); }
    export ptr __mdom_op_sub ( ptr a, ptr b ) { return wrap( dom::op_sub, a, b ); }
    export ptr __mdom_op_mul ( ptr a, ptr b ) { return wrap( dom::op_mul, a, b ); }
    export ptr __mdom_op_sdiv( ptr a, ptr b ) { return wrap( dom::op_sdiv, a, b ); }
    export ptr __mdom_op_udiv( ptr a, ptr b ) { return wrap( dom::op_udiv, a, b ); }
    export ptr __mdom_op_srem( ptr a, ptr b ) { return wrap( dom::op_srem, a, b ); }
    export ptr __mdom_op_urem( ptr a, ptr b ) { return wrap( dom::op_urem, a, b ); }

    export ptr __mdom_op_fadd( ptr a, ptr b ) { return wrap( dom::op_fadd, a, b ); }
    export ptr __mdom_op_fsub( ptr a, ptr b ) { return wrap( dom::op_fsub, a, b ); }
    export ptr __mdom_op_fmul( ptr a, ptr b ) { return wrap( dom::op_fmul, a, b ); }
    export ptr __mdom_op_fdiv( ptr a, ptr b ) { return wrap( dom::op_fdiv, a, b ); }
    export ptr __mdom_op_frem( ptr a, ptr b ) { return wrap( dom::op_frem, a, b ); }

    export ptr __mdom_op_shl ( ptr a, ptr b ) { return wrap( dom::op_shl,  a, b ); }
    export ptr __mdom_op_ashr( ptr a, ptr b ) { return wrap( dom::op_ashr, a, b ); }
    export ptr __mdom_op_lshr( ptr a, ptr b ) { return wrap( dom::op_lshr, a, b ); }
    export ptr __mdom_op_and ( ptr a, ptr b ) { return wrap( dom::op_and,  a, b ); }
    export ptr __mdom_op_or  ( ptr a, ptr b ) { return wrap( dom::op_or,   a, b ); }
    export ptr __mdom_op_xor ( ptr a, ptr b ) { return wrap( dom::op_xor,  a, b ); }

    export ptr __mdom_op_eq ( ptr a, ptr b )  { return wrap( dom::op_eq,  a, b ); }
    export ptr __mdom_op_ne ( ptr a, ptr b )  { return wrap( dom::op_neq, a, b ); }
    export ptr __mdom_op_ugt( ptr a, ptr b )  { return wrap( dom::op_ugt, a, b ); }
    export ptr __mdom_op_uge( ptr a, ptr b )  { return wrap( dom::op_uge, a, b ); }
    export ptr __mdom_op_ult( ptr a, ptr b )  { return wrap( dom::op_ult, a, b ); }
    export ptr __mdom_op_ule( ptr a, ptr b )  { return wrap( dom::op_ule, a, b ); }
    export ptr __mdom_op_sgt( ptr a, ptr b )  { return wrap( dom::op_sgt, a, b ); }
    export ptr __mdom_op_sge( ptr a, ptr b )  { return wrap( dom::op_sge, a, b ); }
    export ptr __mdom_op_slt( ptr a, ptr b )  { return wrap( dom::op_slt, a, b ); }
    export ptr __mdom_op_sle( ptr a, ptr b )  { return wrap( dom::op_sle, a, b ); }

    export ptr __mdom_op_foeq( ptr a, ptr b ) { return wrap( dom::op_foeq, a, b ); }
    export ptr __mdom_op_fogt( ptr a, ptr b ) { return wrap( dom::op_fogt, a, b ); }
    export ptr __mdom_op_foge( ptr a, ptr b ) { return wrap( dom::op_foge, a, b ); }
    export ptr __mdom_op_folt( ptr a, ptr b ) { return wrap( dom::op_folt, a, b ); }
    export ptr __mdom_op_fole( ptr a, ptr b ) { return wrap( dom::op_fole, a, b ); }
    export ptr __mdom_op_ford( ptr a, ptr b ) { return wrap( dom::op_ford, a, b ); }
    export ptr __mdom_op_funo( ptr a, ptr b ) { return wrap( dom::op_funo, a, b ); }
    export ptr __mdom_op_fueq( ptr a, ptr b ) { return wrap( dom::op_fueq, a, b ); }
    export ptr __mdom_op_fugt( ptr a, ptr b ) { return wrap( dom::op_fugt, a, b ); }
    export ptr __mdom_op_fuge( ptr a, ptr b ) { return wrap( dom::op_fuge, a, b ); }
    export ptr __mdom_op_fult( ptr a, ptr b ) { return wrap( dom::op_fult, a, b ); }
    export ptr __mdom_op_fule( ptr a, ptr b ) { return wrap( dom::op_fule, a, b ); }

    export ptr __mdom_op_ffalse( ptr a, ptr b ) { return wrap( dom::op_ffalse, a, b ); }
    export ptr __mdom_op_ftrue ( ptr a, ptr b ) { return wrap( dom::op_ftrue,  a, b ); }

    export ptr __mdom_op_concat ( ptr a, ptr b ) { return wrap( dom::op_concat,  a, b ); }
    export ptr __mdom_op_trunc  ( ptr a, bw  b ) { return wrap( dom::op_trunc,   a, b ); }
    export ptr __mdom_op_fptrunc( ptr a, bw  b ) { return wrap( dom::op_fptrunc, a, b ); }
    export ptr __mdom_op_sitofp ( ptr a, bw  b ) { return wrap( dom::op_sitofp,  a, b ); }
    export ptr __mdom_op_uitofp ( ptr a, bw  b ) { return wrap( dom::op_uitofp,  a, b ); }
    export ptr __mdom_op_zext   ( ptr a, bw  b ) { return wrap( dom::op_zext,    a, b ); }
    export ptr __mdom_op_sext   ( ptr a, bw  b ) { return wrap( dom::op_sext,    a, b ); }
    export ptr __mdom_op_fpext  ( ptr a, bw  b ) { return wrap( dom::op_fpext,   a, b ); }
    export ptr __mdom_op_fptosi ( ptr a, bw  b ) { return wrap( dom::op_fptosi,  a, b ); }
    export ptr __mdom_op_fptoui ( ptr a, bw  b ) { return wrap( dom::op_fptoui,  a, b ); }

    /* TODO aggregate operations */

    export ptr __mdom_op_extract( ptr a, bw s, bw e ) { return wrap( dom::op_extract, a, s, e ); }
}
