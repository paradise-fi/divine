// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>

#include <brick-smt>
#include <type_traits>

namespace abstract
{
    namespace smt = brick::smt;

    struct Term
    {
        using Bitwidth = int8_t;
        using VarID = uint16_t;
        using Op = smt::Op;

        using RPN = smt::RPN< Array< uint8_t, _VM_PT_Marked > >;

        RPN rpn;

        template< typename T >
        inline constexpr std::size_t bitwidth() noexcept
        {
            return std::numeric_limits< T >::digits;
        }

        template< typename T >
        Term( Abstracted< T > ) noexcept;

        template< typename T >
        Term( T value ) noexcept;

        Term( RPN rpn ) : rpn( rpn ) {}

        Term() = default;
        Term( const Term& ) = default;
        Term( Term&& ) = default;

        Term& operator=( const Term& ) = default;
        Term& operator=( Term&& ) = default;

        static inline Term lift_bool( bool v ) noexcept { return Term( v ); }
        static inline Term lift_s( uint8_t v ) noexcept { return Term( v ); }
        static inline Term lift_s( uint16_t v ) noexcept { return Term( v ); }
        static inline Term lift_s( uint32_t v ) noexcept { return Term( v ); }
        static inline Term lift_s( uint64_t v ) noexcept { return Term( v ); }

        template< Op op, Bitwidth bw >
        constexpr Term& apply_in_place( const Term& other ) noexcept
        {
            rpn.extend( other.rpn );
            rpn.apply< op, bw >();
            return *this;
        }

        template< Op op, Bitwidth bw >
        constexpr Term& apply_in_place_op() noexcept
        {
            rpn.apply< op, bw >();
            return *this;
        }

        template< Op op, Bitwidth bw >
        static Term apply( const Term& lhs, const Term& rhs ) noexcept
        {
            Term res( lhs );
            res.rpn.extend( rhs.rpn );
            res.rpn.apply< op, bw >();
            return res;
        }

        template< Op op, Bitwidth bw >
        static Term apply( Term term ) noexcept
        {
            term.rpn.apply< op, bw >();
            return term;
        }

        inline bool empty() const noexcept
        {
            return rpn.empty();
        }

        #define BINARY( name, op, bw ) \
        friend inline Term name ## _ ## bw( const Term &a, const Term &b ) noexcept \
        { \
            return apply< Op::op, bw >( a, b ); \
        }

        #define COMPARISON( name, op ) \
        friend inline Term name( const Term &a, const Term &b ) noexcept \
        { \
            return apply< Op::op, 1 >( a, b ); \
        }

        #define UNARY( name, op, bw ) \
        friend inline Term name ## _ ## bw( const Term &a ) noexcept \
        { \
            return apply< Op::op, bw >( a ); \
        }

        // Arithmetic operations
        #define ARITHMETIC_OP( bw ) \
            BINARY( op_add, BvAdd, bw ) \
            BINARY( op_fadd, FpAdd, bw) \
            BINARY( op_sub, BvSub, bw ) \
            BINARY( op_fsub, FpSub, bw ) \
            BINARY( op_mul, BvMul, bw ) \
            BINARY( op_fmul, FpMul, bw ) \
            BINARY( cf_udiv, BvUDiv, bw ) \
            BINARY( cf_sdiv, BvSDiv, bw ) \
            BINARY( cf_fdiv, FpDiv, bw ) \
            BINARY( cf_urem, BvURem, bw ) \
            BINARY( cf_srem, BvSRem, bw ) \
            BINARY( cf_frem, FpRem, bw )

        ARITHMETIC_OP( 8 )
        ARITHMETIC_OP( 16 )
        ARITHMETIC_OP( 32 )
        ARITHMETIC_OP( 64 )

        // Bitwise operations
        // BINARY( op_shl, BvShl )
        // BINARY( op_lshr, BvLShr )
        // BINARY( op_ashr, BvAShr )
        // BINARY( op_and, BvAnd )
        // BINARY( op_or, BvOr )
        // BINARY( op_xor, BvXor )

        // Logical operations
        UNARY( op_not, Not, 1 )

        // TODO vector operations
        // extract element
        // insert element
        // shuffle vector

        // TODO Memory access operations
        // alloca
        // load
        // store
        // fence
        // cmpxchg
        // atomicrmw
        // getelementptr

        // TODO Coversion operations
        // trunc
        // zext
        // sext
        // fptrunc
        // fpext
        // fptoui
        // fptosi
        // uitofp
        // sitofp
        // ptrtoint
        // inttoptr
        // bitcast
        // addrspacecast
        /* Comparison operations */
        COMPARISON( op_ieq , Eq )
        COMPARISON( op_ine , NE )
        COMPARISON( op_iugt, BvUGT )
        COMPARISON( op_iuge, BvUGE )
        COMPARISON( op_iult, BvULT )
        COMPARISON( op_iule, BvULE )
        COMPARISON( op_isgt, BvSGT )
        COMPARISON( op_isge, BvSGE )
        COMPARISON( op_islt, BvSLT )
        COMPARISON( op_isle, BvSLE )
        COMPARISON( op_ffalse, FpFalse )
        COMPARISON( op_foeq, FpOEQ )
        COMPARISON( op_fogt, FpOGT )
        COMPARISON( op_foge, FpOGE )
        COMPARISON( op_folt, FpOLT )
        COMPARISON( op_fole, FpOLE )
        COMPARISON( op_fone, FpONE )
        COMPARISON( op_ford, FpORD )
        COMPARISON( op_fueq, FpUEQ )
        COMPARISON( op_fugt, FpUGT )
        COMPARISON( op_fuge, FpUGE )
        COMPARISON( op_fult, FpULT )
        COMPARISON( op_fule, FpULE )
        COMPARISON( op_fune, FpUNE )
        COMPARISON( op_funo, FpUNO )
        COMPARISON( op_ftrue, FpTrue )

        Tristate to_tristate() noexcept;

        Term constrain( Term constraint, bool expect ) const noexcept;

        uint8_t * pointer() const noexcept
        {
            return reinterpret_cast< uint8_t * >( rpn._data._data );
        }

        friend Term assume( const Term& value, const Term &constraint, bool expect ) noexcept
        {
            return value.constrain( constraint, expect );
        }

        #undef BINARY
        #undef UNARY
    };

    struct TermState
    {
        uint16_t counter = 0;
        Term constraints;
    };

    extern TermState __term_state;

    template< typename T >
    Term::Term( Abstracted< T > ) noexcept
        : rpn( RPN::Variable{ RPN::var< T >(), __term_state.counter++ } )
    {
        static_assert( std::is_integral_v< T > );
    }

    template< typename T >
    Term::Term( T value ) noexcept
        : rpn( RPN::Constant< T >{ -bitwidth< T >(), value } )
    {
        static_assert( std::is_integral_v< T > );
    }

    static_assert( sizeof( Term ) == 8 );

} // namespace abstract
