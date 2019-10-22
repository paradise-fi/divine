// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/constant.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract {

    _LART_INLINE
    uint8_t domain( void * addr ) noexcept { return *static_cast< uint8_t * >( addr ); } // TODO refactor

    // TODO refactor
    template< typename Domain >
    bool is_domain( uint8_t domain ) noexcept {
        return Domain()._id == domain;
    }

    bool is_constant( uint8_t domain ) noexcept {
        return is_domain< Constant >( domain );
    }

    // c++ wrapper for scalar domains

    template< typename Domain, bool _signed = false, bool floating = false >
    struct Scalar
    {
        static_assert( !(floating && !_signed) && "unsigned floats are not permitted" );

        using value_type = void *;

        value_type _value;

        Scalar( void * ptr ) : _value( ptr ) {}
        Scalar( Domain val ) : _value( static_cast< value_type >( val ) ) {}

        operator value_type() { return _value; }
        operator value_type() const { return _value; }

        operator Domain() { return Domain{ _value }; }
        operator Domain() const { return Domain{ _value }; }

        operator bool() {
            if ( is_constant( domain( _value ) ) )
                return Constant::to_tristate( _value );

            auto res = Domain::to_tristate( *this );
            Domain::assume( *this, *this, res );
            return res;
        }

        template< template< typename, typename > class operation >
        _LART_INLINE static Scalar binary_op( Scalar l, Scalar r ) noexcept
        {
            using Op = operation< Constant, Domain >;

            auto ldom = domain( l );
            auto rdom = domain( r );

            if ( is_constant( ldom ) ) {
                if ( is_constant( rdom ) )
                    return Op::concrete( l, r );
                return Op::abstract( Domain::lift( l ), r );
            } else {
                if ( is_constant( rdom ) )
                    return Op::abstract( l, Domain::lift( r ) );
                return Op::abstract( l, r );
            }
        }

        template< typename T >
        _LART_INLINE T lower() const noexcept
        {
            assert( is_constant( domain( _value ) ) );
            return static_cast< Constant * >( _value )->template lower< T >();
        }

        template< template< typename > class operation >
        inline static constexpr bool is_in_domain = is_detected_v< operation, Domain >;

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
            friend Scalar operator op ( Scalar l, Scalar r ) noexcept \
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
            friend Scalar operator op ( Scalar l, Scalar r ) noexcept \
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

        friend Scalar operator>>( Scalar l, Scalar r ) noexcept
        {
            if constexpr ( !_signed && is_detected_v< op_lshr_t, Domain > ) \
                return binary_op< lshr_op >( l, r );
            else if constexpr ( _signed && is_detected_v< op_ashr_t, Domain > ) \
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
            friend Scalar operator op ( Scalar l, Scalar r ) noexcept \
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
