// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/constant.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract
{
    // c++ wrapper for scalar domains

    template< typename domain_t, bool _signed = false, bool floating = false >
    struct scalar
    {
        static_assert( !(floating && !_signed) && "unsigned floats are not permitted" );

        domain_t _value;

        scalar( domain_t val ) : _value( val ) {}
        operator domain_t() { return _value; }
        operator domain_t() const { return _value; }

        operator bool()
        {
            switch ( domain_t::to_tristate( *this ) )
            {
                case tristate_t::True: return true;
                case tristate_t::False: return false;
            }

            bool rv = __vm_choose( 2 );
            domain_t::assume( *this, *this, rv );
            return rv;
        }
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

        template< typename concrete_t >
        _LART_INLINE static scalar_t lift( concrete_t val ) noexcept
        {
            return domain_t::lift( val );
        }
#endif

        using dom = domain_t;

        static constexpr auto bv_div = _signed ? dom::op_sdiv : dom::op_udiv;
        static constexpr auto bv_rem = _signed ? dom::op_srem : dom::op_urem;

        static constexpr auto add = floating ? dom::op_fadd : dom::op_add;
        static constexpr auto sub = floating ? dom::op_fsub : dom::op_sub;
        static constexpr auto div = floating ? dom::op_fdiv : bv_div;
        static constexpr auto rem = floating ? dom::op_frem : bv_rem;

        scalar operator+( scalar o ) const noexcept { return add( *this, o ); }
        scalar operator-( scalar o ) const noexcept { return sub( *this, o ); }
        scalar operator/( scalar o ) const noexcept { return div( *this, o ); }
        scalar operator%( scalar o ) const noexcept { return rem( *this, o ); }
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
