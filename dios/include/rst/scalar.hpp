// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/constant.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract {

    // c++ wrapper for scalar domains

    template< typename domain_t, bool _signed = false, bool floating = false >
    struct scalar_t
    {
        static_assert( !(floating && !_signed) && "unsigned floats are not permitted" );

        using value_type = void *;

        value_type _value;

        scalar_t( void * ptr ) : _value( ptr ) {}
        scalar_t( domain_t val ) : _value( static_cast< value_type >( val ) ) {}

        operator value_type() { return _value; }
        operator value_type() const { return _value; }

        operator domain_t() { return domain_t{ _value }; }
        operator domain_t() const { return domain_t{ _value }; }

        operator bool() {
            if ( is_constant( domain_t( _value ) ) )
                return constant_t::to_tristate( _value );

            auto res = domain_t::to_tristate( *this );
            domain_t::assume( *this, *this, res );
            return res;
        }

        template< template< typename, typename > class operation >
        _LART_INLINE static scalar_t binary_op( scalar_t l, scalar_t r ) noexcept
        {
            using Op = operation< constant_t, domain_t >;

            auto ldom = domain_t( l );
            auto rdom = domain_t( r );

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
            assert( is_constant( domain_t( _value ) ) );
            return static_cast< constant_t * >( _value )->template lower< concrete_t >();
        }

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
            friend scalar_t operator op ( scalar_t l, scalar_t r ) noexcept \
            { \
                if constexpr ( is_in_domain< op_ ## name ## _t > ) \
                    return binary_op< name ## _op >( l, r ); \
                else \
                    UNREACHABLE( "unsupported operation " # name ); \
            }

        #define operation( name, op ) \
            op_traits( name ) \
            op_traits( s ## name ) \
            op_traits( f ## name ) \
            \
            friend scalar_t operator op ( scalar_t l, scalar_t r ) noexcept \
            { \
                if constexpr ( unsigned_operation( name ) ) \
                    perform_operation( name ) \
                else if constexpr ( signed_operation( name ) ) \
                    perform_signed_operation( name ) \
                else if constexpr ( floating_operation( name ) ) \
                    perform_floating_operation( name ) \
                else \
                    UNREACHABLE( "unsupported operation " # name ); \
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
            friend scalar_t operator op ( scalar_t l, scalar_t r ) noexcept \
            { \
                if constexpr ( unsigned_icmp_operation( name ) ) \
                    perform_unsigned_operation( name ) \
                else if constexpr ( signed_operation( name ) ) \
                    perform_signed_operation( name ) \
                else \
                    UNREACHABLE( "unsupported operation " # name ); \
            }

        icmp_operation( ge, >= )
        icmp_operation( lt, < )
        icmp_operation( le, <= )

        /* cast operations */
    };

} // namespace __dios::rst::abstract
