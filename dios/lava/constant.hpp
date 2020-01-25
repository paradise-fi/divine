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

#include "tristate.hpp"
#include "base.hpp"

#include <brick-assert>

namespace __lava
{
    struct [[gnu::packed]] constant_data
    {
        uint64_t value;
        enum type_t { bv, fp, ptr } type;
        bitwidth_t bw;
    };

    using constant_storage = tagged_storage< constant_data >;

    template< typename T > struct number_type { using type = T; };
    template< bool, int > struct number_;

    template<> struct number_< true, 1 >  : number_type< bool > {};
    template<> struct number_< true, 8 >  : number_type< int8_t > {};
    template<> struct number_< true, 16 > : number_type< int16_t > {};
    template<> struct number_< true, 32 > : number_type< int32_t > {};
    template<> struct number_< true, 64 > : number_type< int64_t > {};

    template<> struct number_< false, 1 >  : number_type< bool > {};
    template<> struct number_< false, 8 >  : number_type< uint8_t > {};
    template<> struct number_< false, 16 > : number_type< uint16_t > {};
    template<> struct number_< false, 32 > : number_type< uint32_t > {};
    template<> struct number_< false, 64 > : number_type< uint64_t > {};

    template< bool s, int w >
    using number = typename number_< s, w >::type;

    struct constant : constant_storage, domain_mixin< constant >
    {
        constant() = default;
        constant( constant_data::type_t t, bitwidth_t bw, uint64_t v )
            : tagged_storage< constant_data >( constant_data{ v, t, bw } )
        {}

        template< typename concrete_t >
        static auto lift( concrete_t value )
            -> std::enable_if_t< std::is_trivial_v< concrete_t >, constant >
        {
            static_assert( sizeof( concrete_t ) <= 8 );

            uint64_t val;
            constant_data::type_t type;

            if constexpr ( std::is_pointer_v< concrete_t > )
            {
                val = reinterpret_cast< uintptr_t >( value );
                type = constant_data::ptr;
            }
            else
                val = value;

            if constexpr ( std::is_integral_v< concrete_t > )
                type = constant_data::bv;
            if constexpr ( std::is_floating_point_v< concrete_t > )
                type = constant_data::fp;

            return { type, bitwidth_v< concrete_t >, val };
        }

        static constant lift( array_ref ) { NOT_IMPLEMENTED(); }

        template< typename > static constant any()
        {
            // UNREACHABLE( "Constant domain does not provide lift_any operation" );
        }

        template< bool _signed, int bw >
        static number< _signed, bw > cast( constant c )
        {
            return c->value;
        }

        template< bool _signed, typename F, typename... val_t  >
        static constant with_type( F f_, const val_t & ... vals )
        {
            bitwidth_t bw = std::max( { vals->bw ... } );
            const auto t_bv = constant_data::bv;
            auto f = [&]( auto... x ) -> uint64_t { return f_( x... ); };

            if ( ( ( vals->type == constant_data::bv ) && ... ) )
                switch ( bw )
                {
                    case  1: return { t_bv, bw, f( cast< _signed,  1 >( vals ) ... ) };
                    case  8: return { t_bv, bw, f( cast< _signed,  8 >( vals ) ... ) };
                    case 16: return { t_bv, bw, f( cast< _signed, 16 >( vals ) ... ) };
                    case 32: return { t_bv, bw, f( cast< _signed, 32 >( vals ) ... ) };
                    case 64: return { t_bv, bw, f( cast< _signed, 64 >( vals ) ... ) };
                }

            __builtin_trap();
        }

        static constexpr auto wtu = []( const auto & ... xs ) { return with_type< false >( xs... ); };
        static constexpr auto wts = []( const auto & ... xs ) { return with_type< true  >( xs... ); };

        static uint64_t mask( bitwidth_t bw )
        {
            return bw ? ( ( 1ull << ( bw - 1 ) ) | mask( bw - 1 ) ) : 0;
        }

        static constant set_bw( constant c, bitwidth_t bw )
        {
            auto rv = c;
            rv->bw = bw;
            rv->value &= mask( bw );
            return rv;
        }

        static tristate to_tristate( const constant &val )
        {
            if ( val->value )
                return { tristate::yes };
            else
                return { tristate::no };
        }

        static constant assume( const constant &v, const constant &c, bool expect )
        {
            if ( c->value != expect )
                __vm_cancel();
            return v;
        }

        using cv = constant;
        using cr = const constant &;

        static cv op_add ( cr a, cr b ) { return wtu( std::plus(), a, b ); }
        static cv op_sub ( cr a, cr b ) { return wtu( std::minus(), a, b ); }
        static cv op_mul ( cr a, cr b ) { return wtu( std::multiplies(), a, b ); }
        static cv op_sdiv( cr a, cr b ) { return wts( std::divides(), a, b ); }
        static cv op_udiv( cr a, cr b ) { return wtu( std::divides(), a, b ); }
        static cv op_srem( cr a, cr b ) { return wts( std::modulus(), a, b ); }
        static cv op_urem( cr a, cr b ) { return wtu( std::modulus(), a, b ); }

        static cv op_and( cr a, cr b ) { return wtu( std::bit_and(), a, b ); }
        static cv op_or ( cr a, cr b ) { return wtu( std::bit_or(), a, b ); }
        static cv op_xor( cr a, cr b ) { return wtu( std::bit_xor(), a, b ); }

        static constexpr auto shl = []( auto a, auto b ) { return a << b; };
        static constexpr auto shr = []( auto a, auto b ) { return a >> b; };

        static cv op_shl ( cr a, cr b ) { return wtu( shl, a, b ); }
        static cv op_ashr( cr a, cr b ) { return wts( shr, a, b ); }
        static cv op_lshr( cr a, cr b ) { return wtu( shr, a, b ); }

        static cv op_eq ( cr a, cr b ) { return wtu( std::equal_to(), a, b ); }
        static cv op_ne ( cr a, cr b ) { return wtu( std::not_equal_to(), a, b ); }
        static cv op_ult( cr a, cr b ) { return wtu( std::less(), a, b ); }
        static cv op_ugt( cr a, cr b ) { return wtu( std::greater(), a, b ); }
        static cv op_ule( cr a, cr b ) { return wtu( std::less_equal(), a, b ); }
        static cv op_uge( cr a, cr b ) { return wtu( std::greater_equal(), a, b ); }

        static cv op_slt( cr a, cr b ) { return wts( std::less(), a, b ); }
        static cv op_sgt( cr a, cr b ) { return wts( std::greater(), a, b ); }
        static cv op_sle( cr a, cr b ) { return wts( std::less_equal(), a, b ); }
        static cv op_sge( cr a, cr b ) { return wts( std::greater_equal(), a, b ); }

        static cv op_trunc( cv c, bw w ) { return set_bw( c, w ); }
        static cv op_zext( cv c, bw w )  { return set_bw( c, w ); }
        static cv op_zfit( cv c, bw w )  { return set_bw( c, w ); }
        static cv op_sext( cv c, bw w )
        {
            return set_bw( wts( []( auto v ) { return int64_t( v ); }, c ), w );
        }

        static void trace( constant ptr, const char * msg = "" )
        {
            __dios_trace_f( "%s%d", msg, ptr->value );
        };
    };

    static_assert( sizeof( constant ) == 8 );
}
