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
        // TODO union of types + store bitwidth

        Value value;

        Constant( Value value )
            : value( value )
        {}

        _LART_INLINE static Value& get_value( Ptr ptr ) noexcept
        {
            return static_cast< Constant * >( ptr )->value;
        }

        template< typename T >
        _LART_INLINE static Ptr lift( T value ) noexcept
        {
            static_assert( sizeof( T ) <= sizeof( Value ) );
            auto ptr = __vm_obj_make( sizeof( Constant ), _VM_PT_Heap );
            new ( ptr ) Constant( value );
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
            _UNREACHABLE_F( "Constant domain does not provide lift_any operation" );
        }

        _LART_INTERFACE
        static Ptr op_thaw( Ptr constant, uint8_t bw ) noexcept
        {
            auto val = get_value( constant );
            if ( bw == 1 )
                return lift( static_cast< bool >( val ) );
            if ( bw == 8 )
                return lift( static_cast< uint8_t >( val ) );
            if ( bw == 16 )
                return lift( static_cast< uint16_t >( val ) );
            if ( bw == 32 )
                return lift( static_cast< uint32_t >( val ) );
            if ( bw == 64 )
                return lift( static_cast< uint64_t >( val ) );

            _UNREACHABLE_F( "Unsupported bitwidth" );
        }

        _LART_INTERFACE
        static Tristate to_tristate( Ptr constant ) noexcept
        {
            if ( get_value( constant ) )
                return { Tristate::True };
            return { Tristate::False };
        }

        _LART_INTERFACE
        static Ptr assume( Ptr /*value*/, Ptr /*constraint*/, bool /*expect*/ ) noexcept
        {
            return nullptr;
        }

        template< typename Op >
        _LART_INLINE static Ptr binary( Ptr lhs, Ptr rhs, Op op ) noexcept
        {
            return lift( op( get_value( lhs ), get_value( rhs ) ) );
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
    };

} // namespace __dios::rst::abstract
