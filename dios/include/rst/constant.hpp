// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/base.hpp>

#include <variant>
#include <brick-assert>

namespace __dios::rst::abstract {

    struct constant_t : tagged_abstract_domain_t
    {
        using value_t = uint64_t;

        enum class type_t : uint8_t { Integer, Float, Pointer };

        type_t type;
        bitwidth_t bw;
        value_t value;

        _LART_INLINE static constant_t& get_constant( abstract_value_t ptr ) noexcept
        {
            return *static_cast< constant_t * >( ptr );
        }

        template< typename concrete_t >
        _LART_NOINLINE static abstract_value_t lift( concrete_t value ) noexcept
        {
            static_assert( sizeof( concrete_t ) <= sizeof( value_t ) );
            auto ptr = __vm_obj_make( sizeof( constant_t ), _VM_PT_Heap );
            new ( ptr ) constant_t();

            auto con = static_cast< constant_t * >( ptr );
            if constexpr ( std::is_pointer_v< concrete_t > ) {
                con->value = reinterpret_cast< uintptr_t >( value );
            } else {
                con->value = value;
            }

            if constexpr ( std::is_integral_v< concrete_t > )
                con->type = type_t::Integer;
            else if constexpr ( std::is_floating_point_v< concrete_t > )
                con->type = type_t::Float;
            else if constexpr ( std::is_pointer_v< concrete_t > )
                con->type = type_t::Pointer;
            else
                static_assert( "unsupported constant type" );

            con->bw = bitwidth< concrete_t >();
            return static_cast< abstract_value_t >( ptr );
        }

        #define __lift( name, concrete_t ) \
            _LART_INTERFACE static abstract_value_t lift_one_ ## name( concrete_t value ) noexcept \
            { \
                return lift( value ); \
            }

        /* abstraction operations */
        __lift( i1, bool )
        __lift( i8, uint8_t )
        __lift( i16, uint16_t )
        __lift( i32, uint32_t )
        __lift( i64, uint64_t )
        __lift( ptr, void * )

        _LART_INTERFACE
        static abstract_value_t lift_any() noexcept
        {
            tagged_abstract_domain_t(); // for LART to detect lift_any
            UNREACHABLE( "Constant domain does not provide lift_any operation" );
        }

        #define PERFORM_LIFT_IF( type ) \
            if ( bw == bitwidth< type >() ) \
                return lift( static_cast< type >( val ) );

        template< typename concrete_t >
        _LART_INLINE concrete_t lower() const noexcept
        {
            return static_cast< concrete_t >( value );
        }

        _LART_INTERFACE
        static abstract_value_t op_thaw( abstract_value_t constant, uint8_t bw ) noexcept
        {
            auto val = get_constant( constant ).value;
            PERFORM_LIFT_IF( bool )
            PERFORM_LIFT_IF( uint8_t )
            PERFORM_LIFT_IF( uint16_t )
            PERFORM_LIFT_IF( uint32_t )
            PERFORM_LIFT_IF( uint64_t )

            UNREACHABLE( "Unsupported bitwidth" );
        }

        _LART_INTERFACE
        static tristate_t to_tristate( abstract_value_t val ) noexcept
        {
            if ( get_constant( val ).value )
                return { tristate_t::True };
            return { tristate_t::False };
        }

        _LART_INTERFACE
        static abstract_value_t assume( abstract_value_t value
                                      , abstract_value_t constraint
                                      , bool expect ) noexcept
        {
            if ( get_constant( constraint ).lower< bool >() != expect )
                __vm_cancel();
            return value;
        }

        #define PERFORM_OP_IF( type ) \
            if ( bw == bitwidth< type >() ) \
                return lift( op( static_cast< type >( l.value ), static_cast< type >( r.value ) ) );

        template< bool _signed = false, typename op_t >
        _LART_INLINE static abstract_value_t binary( abstract_value_t lhs
                                                   , abstract_value_t rhs
                                                   , op_t op ) noexcept
        {
            auto l = get_constant( lhs );
            auto r = get_constant( rhs );
            auto bw = std::max( l.bw, r.bw );
            if constexpr ( _signed ) {
                PERFORM_OP_IF( int8_t )
                PERFORM_OP_IF( int16_t )
                PERFORM_OP_IF( int32_t )
                PERFORM_OP_IF( int64_t )
            } else {
                PERFORM_OP_IF( bool )
                PERFORM_OP_IF( uint8_t )
                PERFORM_OP_IF( uint16_t )
                PERFORM_OP_IF( uint32_t )
                PERFORM_OP_IF( uint64_t )
            }

            UNREACHABLE( "unsupported integer constant bitwidth", bw );
        }

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

        /* arithmetic operations */
        __bin( op_add, std::plus )
        //__bin( op_fadd )
        __bin( op_sub, std::minus )
        //__bin( op_fsub )
        __bin( op_mul, std::multiplies )
        //__bin( op_fmul )
        __bin( op_udiv, std::divides )
        __sbin( op_sdiv, std::divides )
        //__bin( op_fdiv )
        __bin( op_urem, std::modulus )
        __sbin( op_srem, std::modulus )
        //__bin( op_frem )

        /* bitwise operations */
        __bin( op_shl, op::shift_left )
        __bin( op_lshr, op::shift_right )
        __sbin( op_ashr, op::arithmetic_shift_right )
        __bin( op_and, std::bit_and )
        __bin( op_or, std::bit_or )
        __bin( op_xor, std::bit_xor )

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

        static void trace( abstract_value_t ptr, const char * msg = "" ) noexcept
        {
            __dios_trace_f( "%s%d", msg, get_constant( ptr ).value );
        };

    } __attribute__((packed));

    static_assert( sizeof( constant_t ) == 11 );

    _LART_INLINE
    static bool is_constant( uint8_t domain ) noexcept
    {
        return is_domain< constant_t >( domain );
    }

} // namespace __dios::rst::abstract
