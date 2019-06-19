// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>
#include <rst/base.hpp>

#include <brick-smt>
#include <type_traits>

namespace __dios::rst::abstract {
    namespace smt = brick::smt;

    using RPN = smt::RPN< Array< uint8_t, _VM_PT_Marked > >;

    template< typename T >
    _LART_INLINE constexpr std::size_t bitwidth() noexcept
    {
        return std::numeric_limits< T >::digits;
    }

    struct Term
    {
        void * pointer = nullptr;

        using Op = smt::Op;

        _LART_INLINE RPN& as_rpn() noexcept
        {
            return *reinterpret_cast< RPN * >( &pointer );
        }

        _LART_INLINE const RPN& as_rpn() const noexcept
        {
            return *reinterpret_cast< const RPN * >( &pointer );
        }

        template< Op op >
        constexpr Term apply_in_place( const Term& other ) noexcept
        {
            this->as_rpn().extend( other.as_rpn() );
            this->as_rpn().apply< op >();
            return *this;
        }

        template< Op op >
        constexpr Term apply_in_place_op() noexcept
        {
            this->as_rpn().apply< op >();
            return *this;
        }

        explicit operator void*() const { return pointer; }

        template< typename T >
        _LART_INLINE static RPN::Constant< T > constant( T value ) noexcept
        {
            return { -bitwidth< T >(), value };
        }

        template< typename T >
        _LART_INTERFACE static Term lift_any( Abstracted< T > ) noexcept;

        template< typename T >
        _LART_INLINE static Term lift( T value ) noexcept
        {
            auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
            new ( ptr ) Base();
            Term term{ ptr };
            term.as_rpn().extend( constant( value ) );
            return term;
        }

        #define __lift( name, T ) \
            _LART_INTERFACE static Term lift_one_ ## name( T value ) noexcept { \
                return lift( value ); \
            }

        /* abstraction operations */
        __lift( i1, bool )
        __lift( i8, uint8_t )
        __lift( i16, uint16_t )
        __lift( i32, uint32_t )
        __lift( i64, uint64_t )

        __lift( float, float )
        __lift( double, double )

        _LART_INTERFACE
        static Tristate to_tristate( Term ) noexcept
        {
            return { Tristate::Unknown };
        }

        template< Op op >
        static Term binary( Term lhs, Term rhs ) noexcept
        {
            assert( false );
        }

        template< Op op >
        static Term unary( Term arg ) noexcept
        {
            assert( false );
        }

        template< Op op >
        _LART_INLINE static Term cmp( Term lhs, Term rhs ) noexcept
        {
            auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
            new ( ptr ) Base();
            Term term{ ptr };
            term.as_rpn().extend( lhs.as_rpn() );
            term.as_rpn().extend( rhs.as_rpn() );
            term.as_rpn().apply< op >();
            return term;
        }

        template< Op op >
        static Term cast( Term arg, Bitwidth bw ) noexcept
        {
            assert( false );
        }

        _LART_INLINE
        Term constrain( Term constraint, bool expect ) const noexcept;

        _LART_INTERFACE
        Term static assume( Term value, Term constraint, bool expect ) noexcept
        {
            return value.constrain( constraint, expect );
        }

        #define __bin( name, op ) \
            _LART_INTERFACE static Term name( Term lhs, Term rhs ) noexcept { \
                return binary< Op::op >( lhs, rhs ); \
            }

        #define __un( name, op ) \
            _LART_INTERFACE static Term name( Term arg ) noexcept { \
                return unary< Op::op >( arg ); \
            }

        #define __cmp( name, op ) \
            _LART_INTERFACE static Term name( Term lhs, Term rhs ) noexcept { \
                return cmp< Op::op >( lhs, rhs ); \
            }

        #define __cast( name, op ) \
            _LART_INTERFACE static Term name( Term arg, Bitwidth bw ) noexcept { \
                return cast< Op::op >( arg, bw ); \
            }

        /* arithmetic operations */
        __bin( op_add, BvAdd )
        __bin( op_fadd, FpAdd )
        __bin( op_sub, BvSub )
        __bin( op_fsub, FpSub )
        __bin( op_mul, BvMul )
        __bin( op_fmul, FpMul )
        __bin( op_udiv, BvUDiv )
        __bin( op_sdiv, BvSDiv )
        __bin( op_fdiv, FpDiv )
        __bin( op_urem, BvURem )
        __bin( op_srem, BvSRem )
        __bin( op_frem, FpRem )

        /* logic operations */
        // TODO __un( op_fneg )

        /* bitwise operations */
        __bin( op_shl, BvShl )
        __bin( op_lshr, BvLShr )
        __bin( op_ashr, BvAShr )
        __bin( op_and, BvAnd )
        __bin( op_or, BvOr )
        __bin( op_xor, BvXor )

        /* comparison operations */
        __cmp( op_ffalse, FpFalse )
        __cmp( op_foeq, FpOEQ )
        __cmp( op_fogt, FpOGT )
        __cmp( op_foge, FpOGE )
        __cmp( op_folt, FpOLT )
        __cmp( op_fole, FpOLE )
        __cmp( op_fone, FpONE )
        __cmp( op_ford, FpORD )
        __cmp( op_funo, FpUNO )
        __cmp( op_fueq, FpUEQ )
        __cmp( op_fugt, FpUGT )
        __cmp( op_fuge, FpUGE )
        __cmp( op_fult, FpULT )
        __cmp( op_fule, FpULE )
        __cmp( op_fune, FpUNE )
        __cmp( op_ftrue, FpTrue )
        __cmp( op_eq, Eq );
        __cmp( op_ne, NE );
        __cmp( op_ugt, BvUGT );
        __cmp( op_uge, BvUGE );
        __cmp( op_ult, BvULT );
        __cmp( op_ule, BvULE );
        __cmp( op_sgt, BvSGT );
        __cmp( op_sge, BvSGE );
        __cmp( op_slt, BvSLT );
        __cmp( op_sle, BvSLE );


        /* cast operations */
        /*__cast( op_fpext );
        __cast( op_fptosi );
        __cast( op_fptoui );
        __cast( op_fptrunc );
        __cast( op_inttoptr );
        __cast( op_ptrtoint );
        __cast( op_sext );
        __cast( op_sitofp );
        __cast( op_trunc );
        __cast( op_uitofp );
        __cast( op_zext );*/

        #undef __un
        #undef __bin
        #undef __cmp
        #undef __cast
        #undef __lift
    };

    struct TermState
    {
        uint16_t counter = 0;
        Term constraints;
    };

    extern TermState __term_state;

    template< typename T >
    _LART_INLINE RPN::Variable variable() noexcept
    {
        return { RPN::var< T >(), __term_state.counter++ };
    }

    template< typename T >
    _LART_INTERFACE Term Term::lift_any( Abstracted< T > ) noexcept
    {
        auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
        new ( ptr ) Base();
        Term term{ ptr };
        term.as_rpn().extend( variable< T >() );
        return term;
    }

    static_assert( sizeof( Term ) == 8 );

} // namespace abstract
