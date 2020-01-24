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

            return { type, sizeof( concrete_t ) * 8, val };
        }

        static constant lift_any()
        {
            // UNREACHABLE( "Constant domain does not provide lift_any operation" );
        }

        template< typename T >
        uint64_t trunc( uint64_t v ) noexcept
        {
            return static_cast< T >( v );
        }

        template< typename F, typename... val_t  >
        static constant wtu( F f_, const val_t & ... vals ) noexcept
        {
            bitwidth_t bw = std::max( { vals->bw ... } );
            const auto t_bv = constant_data::bv;
            auto f = [&]( auto... x ) -> uint64_t { return f_( x... ); };

            if ( ( ( vals->type == constant_data::bv ) && ... ) )
                switch ( bw )
                {
                    case  1: return { t_bv, bw, f( static_cast< bool >( vals->value ) ... ) };
                    case  8: return { t_bv, bw, f( static_cast< uint8_t >( vals->value ) ... ) };
                    case 16: return { t_bv, bw, f( static_cast< uint16_t >( vals->value ) ... ) };
                    case 32: return { t_bv, bw, f( static_cast< uint32_t >( vals->value ) ... ) };
                    case 64: return { t_bv, bw, f( static_cast< uint64_t >( vals->value ) ... ) };
                }

            __builtin_trap();
            // NOT_IMPLEMENTED();
        }

        template< typename F, typename... val_t  >
        static constant wts( F, const val_t & ... ) noexcept
        {
            __builtin_trap();
            // NOT_IMPLEMENTED();
        }

        static constant op_thaw( constant c, uint8_t bw ) noexcept
        {
            constant d = c;
            d->bw = bw;
            wtu( [&]( auto f ) noexcept { return d->bw = f; }, d );
            return d;
        }

        static tristate to_tristate( const constant &val ) noexcept
        {
            if ( val->value )
                return { tristate::yes };
            else
                return { tristate::no };
        }

        static constant assume( const constant &v, const constant &c, bool expect ) noexcept
        {
            if ( c->value != expect )
                __vm_cancel();
            return v;
        }
#if 0
        #define PERFORM_CAST_IF( type ) \
            if ( bw == bitwidth< type >() ) \
                return lift( static_cast< type >( v ) );

        #define TRUNC_TO_IF( type ) \
            if ( con.bw == bitwidth< type >() ) \
                return static_cast< type >( con.value );

        template< bool _signed >
        _LART_INLINE static auto trunc_to_value( const constant_t & con ) noexcept
            -> std::enable_if_t< _signed , int64_t >
        {
            TRUNC_TO_IF( int8_t )
            TRUNC_TO_IF( int16_t )
            TRUNC_TO_IF( int32_t )
            TRUNC_TO_IF( int64_t )
        }

        template< bool _signed >
        _LART_INLINE static auto trunc_to_value( const constant_t & con ) noexcept
            -> std::enable_if_t< !_signed, uint64_t >
        {
            TRUNC_TO_IF( bool )
            TRUNC_TO_IF( uint8_t )
            TRUNC_TO_IF( uint16_t )
            TRUNC_TO_IF( uint32_t )
            TRUNC_TO_IF( uint64_t )
        }

        template< bool _signed = false >
        _LART_INLINE static abstract_value_t cast( abstract_value_t val, bitwidth_t bw ) noexcept
        {
            auto v = trunc_to_value< _signed >( get_constant( val ) );

            if constexpr ( _signed ) {
                PERFORM_CAST_IF( int8_t )
                PERFORM_CAST_IF( int16_t )
                PERFORM_CAST_IF( int32_t )
                PERFORM_CAST_IF( int64_t )
            } else {
                PERFORM_CAST_IF( bool )
                PERFORM_CAST_IF( uint8_t )
                PERFORM_CAST_IF( uint16_t )
                PERFORM_CAST_IF( uint32_t )
                PERFORM_CAST_IF( uint64_t )
            }

            UNREACHABLE( "unsupported integer constant bitwidth", bw );
        }

        #define __bin( name, op ) \
            _LART_INTERFACE static abstract_value_t name( abstract_value_t lhs, abstract_value_t rhs ) noexcept \
            { \
                return binary( lhs, rhs, op() ); \
            }

        #define __sbin( name, op ) \
            _LART_INTERFACE static abstract_value_t name( abstract_value_t lhs, abstract_value_t rhs ) noexcept \
            { \
                return binary< true /* signed */ >( lhs, rhs, op() ); \
            }

        #define __cast( name, _signed ) \
            _LART_INTERFACE static abstract_value_t name( abstract_value_t val, bitwidth_t bw ) noexcept \
            { \
                return cast< _signed >( val, bw ); \
            }
#endif
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

        static cv op_eq( cr a, cr b ) { return wtu( std::equal_to(), a, b ); }
        static cv op_ne( cr a, cr b ) { return wtu( std::not_equal_to(), a, b ); }

        // op_fadd
        // op_fsub
        // op_fmul
        // op_fdiv
        // op_frem
#if 0
        /* bitwise operations */
        __bin( op_shl, op::shift_left )
        __bin( op_lshr, op::shift_right )
        __sbin( op_ashr, op::arithmetic_shift_right )

        /* comparison operations */
        //__bin( op_ffalse );
        //__bin( op_foeq );
        //__bin( op_fogt );
        //__bin( op_foge );
        //__bin( op_folt );
        //__bin( op_fole );
        //__bin( op_fone );
        //__bin( op_ford );
        //__bin( op_funo );
        //__bin( op_fueq );
        //__bin( op_fugt );
        //__bin( op_fuge );
        //__bin( op_fult );
        //__bin( op_fule );
        //__bin( op_fune );
        //__bin( op_ftrue );

        __bin( op_eq, std::equal_to );
        __bin( op_ne, std::not_equal_to );
        __bin( op_ugt, std::greater );
        __bin( op_uge, std::greater_equal );
        __bin( op_ult, std::less );
        __bin( op_ule, std::less_equal );
        __sbin( op_sgt, std::greater );
        __sbin( op_sge, std::greater_equal );
        __sbin( op_slt, std::less );
        __sbin( op_sle, std::less_equal );

        /* cast operations */
        __cast( op_sext, true /* signed */ );
        __cast( op_zext, false /* unsigned */ );
        __cast( op_trunc, false /* unsigned */ );
        //__cast( op_fpext, FPExt );
        //__cast( op_fptosi, FPToSInt );
        //__cast( op_fptoui, FPToUInt );
        //__cast( op_fptrunc, FPTrunc );
        //__cast( op_inttoptr );
        //__cast( op_ptrtoint );
        //__cast( op_sitofp, SIntToFP );
        //__cast( op_uitofp, UIntToFP );
#endif
        static void trace( constant ptr, const char * msg = "" ) noexcept
        {
            __dios_trace_f( "%s%d", msg, ptr->value );
        };
    };

    static_assert( sizeof( constant ) == 8 );
}
