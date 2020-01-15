// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2020 Petr Ročkai <code@fixp.eu>
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
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

#include <rst/tristate.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract
{
    struct unit;
    // extern unit common_unit;

    struct unit : tagged_array<>, domain_mixin< unit >
    {
        __noinline unit() {}

        template< typename type > static unit lift( type ) { return {}; }
        template< typename type > static unit lift_any( Abstracted< type > ) { return {}; }

        __lartop static unit lift_any() noexcept { return {}; }
        __lartop static unit lift_any_ptr() noexcept { return {}; }

        __lartop static unit op_thaw( unit, uint8_t ) { return {}; }
        __lartop static unit op_load( unit, uint8_t ) { return {}; }
        __lartop static unit op_store( unit, unit, uint8_t ) { return {}; }

        __lartop static unit op_insertvalue( unit, unit, uint64_t ) { return {}; }
        __lartop static unit op_extractvalue( unit, uint64_t ) { return {}; }
        __lartop static unit op_gep( size_t, unit, unit ) { return {}; }
        __lartop static unit assume( unit, unit, bool ) noexcept { return {}; }

        __lartop static tristate_t to_tristate( unit ) noexcept { return { tristate_t::Unknown }; }

        /* arithmetic operations */
        __lartop static unit op_add ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fadd( unit, unit ) noexcept { return {}; }
        __lartop static unit op_sub ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fsub( unit, unit ) noexcept { return {}; }
        __lartop static unit op_mul ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fmul( unit, unit ) noexcept { return {}; }
        __lartop static unit op_udiv( unit, unit ) noexcept { return {}; }
        __lartop static unit op_sdiv( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fdiv( unit, unit ) noexcept { return {}; }
        __lartop static unit op_urem( unit, unit ) noexcept { return {}; }
        __lartop static unit op_srem( unit, unit ) noexcept { return {}; }
        __lartop static unit op_frem( unit, unit ) noexcept { return {}; }

        __lartop static unit op_fneg( unit ) noexcept { return {}; }

        /* bitwise operations */
        __lartop static unit op_shl ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_lshr( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ashr( unit, unit ) noexcept { return {}; }
        __lartop static unit op_and ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_or  ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_xor ( unit, unit ) noexcept { return {}; }

        using bw = uint8_t;

        /* comparison operations */
        __lartop static unit op_foeq( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fogt( unit, unit ) noexcept { return {}; }
        __lartop static unit op_foge( unit, unit ) noexcept { return {}; }
        __lartop static unit op_folt( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fole( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fone( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ford( unit, unit ) noexcept { return {}; }
        __lartop static unit op_funo( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fueq( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fugt( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fuge( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fult( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fule( unit, unit ) noexcept { return {}; }
        __lartop static unit op_fune( unit, unit ) noexcept { return {}; }

        __lartop static unit op_eq ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ne ( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ugt( unit, unit ) noexcept { return {}; }
        __lartop static unit op_uge( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ult( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ule( unit, unit ) noexcept { return {}; }
        __lartop static unit op_sgt( unit, unit ) noexcept { return {}; }
        __lartop static unit op_sge( unit, unit ) noexcept { return {}; }
        __lartop static unit op_slt( unit, unit ) noexcept { return {}; }
        __lartop static unit op_sle( unit, unit ) noexcept { return {}; }

        __lartop static unit op_ffalse( unit, unit ) noexcept { return {}; }
        __lartop static unit op_ftrue( unit, unit ) noexcept { return {}; }


        __lartop static unit op_fpext( unit, bw ) noexcept { return {}; }
        __lartop static unit op_fptosi( unit, bw ) noexcept { return {}; }
        __lartop static unit op_fptoui( unit, bw ) noexcept { return {}; }
        __lartop static unit op_fptrunc( unit, bw ) noexcept { return {}; }
        __lartop static unit op_inttoptr( unit, bw ) noexcept { return {}; }
        __lartop static unit op_ptrtoint( unit, bw ) noexcept { return {}; }
        __lartop static unit op_sext( unit, bw ) noexcept { return {}; }
        __lartop static unit op_sitofp( unit, bw ) noexcept { return {}; }
        __lartop static unit op_trunc( unit, bw ) noexcept { return {}; }
        __lartop static unit op_uitofp( unit, bw ) noexcept { return {}; }
        __lartop static unit op_zext( unit, bw ) noexcept { return {}; }

        /* TODO remove, only here to satisfy the vtable generator */
        __lartop static unit lift_one_i1( i1 v )   noexcept { return lift( v ); }
        __lartop static unit lift_one_i8( i8 v )   noexcept { return lift( v ); }
        __lartop static unit lift_one_i16( i16 v ) noexcept { return lift( v ); }
        __lartop static unit lift_one_i32( i32 v ) noexcept { return lift( v ); }
        __lartop static unit lift_one_i64( i64 v ) noexcept { return lift( v ); }
        __lartop static unit lift_one_f32( f32 v ) noexcept { return lift( v ); }
        __lartop static unit lift_one_f64( f64 v ) noexcept { return lift( v ); }

        __lartop static unit lift_one_ptr( void *v ) noexcept
        {
            return lift( reinterpret_cast< uintptr_t >( v ) );
        }
    };

} // namespace __dios::rst::abstract
