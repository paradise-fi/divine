// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
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

#pragma once
#include "tristate.hpp"
#include <sys/trace.h>

namespace __lava
{
    // c++ wrapper for scalar domains

    template< typename dom, bool _signed = false, bool floating = false >
    struct scalar
    {
        static_assert( !(floating && !_signed) && "unsigned floats are not permitted" );

        dom _value;

        scalar( dom &&val ) : _value( std::move( val ) ) {}
        scalar( const dom &val ) : _value( val ) {}
        operator dom&() { return _value; }
        operator dom&() const { return _value; }

        explicit operator bool()
        {
            tristate v = dom::to_tristate( _value );

            switch ( v.value )
            {
                case tristate::no:  return false;
                case tristate::yes: return true;
                case tristate::maybe:
                    bool rv = __vm_choose( 2 );
                    dom::assume( *this, *this, rv );
                    return rv;
            }
        }

        template< typename type > static scalar lift( type val ) { return { dom::lift( val ) }; }
        template< typename type > static scalar any() { return { dom::template any< type >() }; }

#if 0
        template< template< typename, typename > class operation >
        _LART_INLINE static scalar_t binary_op( scalar_t l, scalar_t r ) noexcept
        {
            using Op = operation< constant_t, domain_t >;

            auto ldom = domain( l );
            auto rdom = domain( r );

            if ( is_constant( ldom ) ) {
                if ( is_constant( rdom ) )
                    return Op::concrete( l, r );
                return Op::abstract( domain_t::lift( l ), r );
            } else {
                if ( is_constant( rdom ) )
                    return Op::abstract( l, domain_t::lift( r ) );
                return Op::abstract( l, r );
            }
        }

        template< typename concrete_t >
        _LART_INLINE concrete_t lower() const noexcept
        {
            assert( is_constant( domain( _value ) ) );
            return static_cast< constant_t * >( _value )->template lower< concrete_t >();
        }

#endif

        static constexpr auto bv_div = _signed ? dom::op_sdiv : dom::op_udiv;
        static constexpr auto bv_rem = _signed ? dom::op_srem : dom::op_urem;

        static constexpr auto ge = _signed ? dom::op_sge : dom::op_uge;
        static constexpr auto le = _signed ? dom::op_sle : dom::op_ule;
        static constexpr auto lt = _signed ? dom::op_slt : dom::op_ult;
        static constexpr auto gt = _signed ? dom::op_sgt : dom::op_ugt;

        static constexpr auto add = floating ? dom::op_fadd : dom::op_add;
        static constexpr auto sub = floating ? dom::op_fsub : dom::op_sub;
        static constexpr auto div = floating ? dom::op_fdiv : bv_div;
        static constexpr auto rem = floating ? dom::op_frem : bv_rem;

        scalar operator+( scalar o ) const noexcept { return add( _value, o._value ); }
        scalar operator-( scalar o ) const noexcept { return sub( _value, o._value ); }
        scalar operator/( scalar o ) const noexcept { return div( _value, o._value ); }
        scalar operator%( scalar o ) const noexcept { return rem( _value, o._value ); }

        scalar operator==( scalar o ) const noexcept { return dom::op_eq( _value, o._value ); }
        scalar operator!=( scalar o ) const noexcept { return dom::op_ne( _value, o._value ); }
        scalar operator>=( scalar o ) const noexcept { return ge( _value, o._value ); }
        scalar operator<=( scalar o ) const noexcept { return le( _value, o._value ); }
        scalar operator< ( scalar o ) const noexcept { return lt( _value, o._value ); }
        scalar operator> ( scalar o ) const noexcept { return gt( _value, o._value ); }

        scalar zfit( int w ) const { return dom::op_zfit( _value, w );; }
#if 0
        template< template< typename > class operation >
        inline static constexpr bool is_in_domain = is_detected_v< operation, domain_t >;

        #define op_traits( name ) \
            template < typename T > \
            using op_ ## name ## _t = decltype( &T::op_ ## name ); \
            \
            template< typename Concrete, typename Abstract > \
            struct name ## _op \
            { \
                static_assert( is_detected_v< op_ ## name ## _t, Concrete > ); \
                static_assert( is_detected_v< op_ ## name ## _t, Abstract > ); \
                inline static constexpr auto concrete = &Concrete::op_ ## name; \
                inline static constexpr auto abstract = &Abstract::op_ ## name; \
            };

        #define unsigned_operation( name ) \
            !_signed && is_in_domain< op_ ## name ## _t >

        #define unsigned_icmp_operation( name ) \
            !_signed && is_in_domain< op_u ## name ## _t >

        #define signed_operation( name ) \
            _signed && is_in_domain< op_s ## name ## _t >

        #define floating_operation( name ) \
            floating && is_in_domain< op_f ## name ## _t >

        #define perform_operation( name ) \
            return binary_op< name ## _op >( l, r );

        #define perform_unsigned_operation( name ) \
            perform_operation( u ## name )

        #define perform_signed_operation( name ) \
            perform_operation( s ## name )

        #define perform_floating_operation( name ) \
            perform_operation( f ## name )

        #define general_operation( name, op ) \
            op_traits( name ) \
            \
            \
            template< typename first_t, typename second_t > \
            _LART_INLINE \
            static scalar_t name ## _impl( first_t l, second_t r ) noexcept \
            { \
                if constexpr ( is_in_domain< op_ ## name ## _t > ) \
                    return binary_op< name ## _op >( l, r ); \
                else \
                    UNREACHABLE( "unsupported operation " # name ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( scalar_t l, scalar_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( abstract_value_t l, scalar_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( scalar_t l, abstract_value_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            }

        #define operation( name, op ) \
            op_traits( name ) \
            op_traits( s ## name ) \
            op_traits( f ## name ) \
            \
            template< typename first_t, typename second_t > \
            _LART_INLINE \
            static auto name ## _impl( first_t l, second_t r ) noexcept \
            { \
                if constexpr ( unsigned_operation( name ) ) \
                    perform_operation( name ) \
                else if constexpr ( signed_operation( name ) ) \
                    perform_signed_operation( name ) \
                else if constexpr ( floating_operation( name ) ) \
                    perform_floating_operation( name ) \
                else \
                    UNREACHABLE( "unsupported operation " # name ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( scalar_t l, scalar_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( abstract_value_t l, scalar_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( scalar_t l, abstract_value_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            }

        /* arithmetic operations */
        operation( add, + )
        operation( sub, - )
        operation( mul, * )
        operation( div, / )
        operation( rem, % )

        /* bitwise operations */
        general_operation( shl, << )

        op_traits( lshr )
        op_traits( ashr )

        _LART_INLINE
        friend scalar_t operator>>( scalar_t l, scalar_t r ) noexcept
        {
            if constexpr ( !_signed && is_detected_v< op_lshr_t, domain_t > ) \
                return binary_op< lshr_op >( l, r );
            else if constexpr ( _signed && is_detected_v< op_ashr_t, domain_t > ) \
                return binary_op< ashr_op >( l, r );
            else
                UNREACHABLE( "unsupported operation lshr/ashr" );
        }

        general_operation( and, & )
        general_operation( or, | )
        general_operation( xor, ^ )

        /* comparison operations */
        general_operation( eq, == )
        general_operation( ne, != )

        #define icmp_operation( name, op ) \
            op_traits( u ## name ) \
            op_traits( s ## name ) \
            \
            template< typename first_t, typename second_t > \
            _LART_INLINE \
            static scalar_t name ## _impl ( first_t l, second_t r ) noexcept \
            { \
                if constexpr ( unsigned_icmp_operation( name ) ) \
                    perform_unsigned_operation( name ) \
                else if constexpr ( signed_operation( name ) ) \
                    perform_signed_operation( name ) \
                else \
                    UNREACHABLE( "unsupported operation " # name ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( scalar_t l, scalar_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( abstract_value_t l, scalar_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            } \
            _LART_INLINE \
            friend scalar_t operator op ( scalar_t l, abstract_value_t r ) noexcept \
            { \
                return name ## _impl( l, r ); \
            }

        icmp_operation( gt, > )
        icmp_operation( ge, >= )
        icmp_operation( lt, < )
        icmp_operation( le, <= )
#endif
    };
}
