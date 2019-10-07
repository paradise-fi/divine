// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/base.hpp>

#include <variant>

namespace __dios::rst::abstract {

    /* Unit/Star domain contains a single value, everything is abstracted to a Star. */
    struct Constant : Base
    {
        using Ptr = void *;
        using Value = uint64_t;
        using Bitwidth = uint8_t;

        enum class Type : uint8_t { Integer, Float, Pointer };

        Type type;
        Bitwidth bw;
        Value value;

        template< typename T >
        _LART_INLINE static constexpr Bitwidth bitwidth() noexcept
        {
            return std::numeric_limits< T >::digits;
        }

        _LART_INLINE static Constant& get_constant( Ptr ptr ) noexcept
        {
            return *static_cast< Constant * >( ptr );
        }

        template< typename T >
        _LART_INTERFACE static Ptr lift( T value ) noexcept
        {
            static_assert( sizeof( T ) <= sizeof( Value ) );
            auto ptr = __vm_obj_make( sizeof( Constant ), _VM_PT_Heap );
            new ( ptr ) Constant();

            auto con = static_cast< Constant * >( ptr );
            con->value = value;

            if constexpr ( std::is_integral_v< T > )
                con->type = Type::Integer;
            else if constexpr ( std::is_floating_point_v< T > )
                con->type = Type::Float;
            else if constexpr ( std::is_pointer_v< T > )
                con->type = Type::Pointer;
            else
                static_assert( "unsupported constant type" );

            con->bw = bitwidth< T >();

            return ptr;
        }

        #define __lift( name, T ) \
            _LART_INTERFACE static Ptr lift_one_ ## name( T value ) noexcept { \
                return lift( value ); \
            }

        /* abstraction operations */
        __lift( i1, bool )
        __lift( i8, uint8_t )
        __lift( i16, uint16_t )
        __lift( i32, uint32_t )
        __lift( i64, uint64_t )

        _LART_INTERFACE
        static Ptr lift_any() noexcept
        {
            Base(); // for LART to detect lift_any
            UNREACHABLE( "Constant domain does not provide lift_any operation" );
        }

        #define PERFORM_LIFT_IF( type ) \
            if ( bw == bitwidth< type >() ) \
                return lift( static_cast< type >( val ) );

        _LART_INTERFACE
        static Ptr op_thaw( Ptr constant, uint8_t bw ) noexcept
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
        static Tristate to_tristate( Ptr ptr ) noexcept
        {
            if ( get_constant( ptr ).value )
                return { Tristate::True };
            return { Tristate::False };
        }

        _LART_INTERFACE
        static Ptr assume( Ptr /*value*/, Ptr /*constraint*/, bool /*expect*/ ) noexcept
        {
            return nullptr;
        }


        #define PERFORM_OP_IF( type ) \
            if ( bw == bitwidth< type >() ) \
                return lift( op( static_cast< type >( l.value ), static_cast< type >( r.value ) ) );

        template< typename Op >
        _LART_INLINE static Ptr binary( Ptr lhs, Ptr rhs, Op op ) noexcept
        {
            auto l = get_constant( lhs );
            auto r = get_constant( rhs );
            auto bw = std::max( l.bw, r.bw );

            PERFORM_OP_IF( bool )
            PERFORM_OP_IF( uint8_t )
            PERFORM_OP_IF( uint16_t )
            PERFORM_OP_IF( uint32_t )
            PERFORM_OP_IF( uint64_t )

            UNREACHABLE( "unsupported integer constant bitwidth", bw );
        }

        #define __bin( name, op ) \
            _LART_INTERFACE static Ptr name( Ptr lhs, Ptr rhs ) noexcept { \
                return binary( lhs, rhs, op() ); \
            }

        /* arithmetic operations */
        __bin( op_add, std::plus )
        //__bin( op_fadd )
        __bin( op_sub, std::minus )
        //__bin( op_fsub )
        __bin( op_mul, std::multiplies )
        //__bin( op_fmul )
        __bin( op_udiv, std::divides )
        __bin( op_sdiv, std::divides )
        //__bin( op_fdiv )
        __bin( op_urem, std::modulus )
        __bin( op_srem, std::modulus )
        //__bin( op_frem )

        /* bitwise operations */
        //__bin( op_shl )
        //__bin( op_lshr )
        //__bin( op_ashr )
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
        //__bin( op_sgt );
        //__bin( op_sge );
        //__bin( op_slt );
        //__bin( op_sle );

        /* cast operations */
        //__cast( op_fpext, FPExt );
        //__cast( op_fptosi, FPToSInt );
        //__cast( op_fptoui, FPToUInt );
        //__cast( op_fptrunc, FPTrunc );
        //__cast( op_inttoptr );
        //__cast( op_ptrtoint );
        //__cast( op_sext, SExt );
        //__cast( op_sitofp, SIntToFP );
        //__cast( op_trunc, Trunc );
        //__cast( op_uitofp, UIntToFP );
        //__cast( op_zext, ZExt );
    } __attribute__((packed));

    static_assert( sizeof( Constant ) == 11 );

} // namespace __dios::rst::abstract
