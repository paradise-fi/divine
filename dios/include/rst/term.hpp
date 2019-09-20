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

    /* Values in the Term abstract domain are built out of Constants, Variables and
     * operations defined on them. Integers are represented as bitvectors of fixed
     * bitwidth, operations similarly have a bitwidth specified (of the arguments they
     * take). See `bricks/brick-smt` for details on the types and operations.
     *
     * A Term is stored/encoded as bytecode in the `pointer` data member, which
     * is a pointer to an RPN structure (RPN is also defined in `brick-smt`). */
    struct Term
    {
        void * pointer = nullptr;     // RPN * ?

        using Op = smt::Op;

        _LART_INLINE RPN& as_rpn() noexcept
        {
            return *reinterpret_cast< RPN * >( &pointer );
        }

        _LART_INLINE const RPN& as_rpn() const noexcept
        {
            return *reinterpret_cast< const RPN * >( &pointer );
        }

        template< typename T >
        _LART_INLINE constexpr Term extend( const T& value ) noexcept
        {
            this->as_rpn().extend( value );
            return *this;
        }

        _LART_INLINE constexpr Term extend( const Term& other ) noexcept
        {
            return extend( other.as_rpn() );
        }

        template< Op op >
        _LART_INLINE constexpr Term apply() noexcept
        {
            this->as_rpn().apply< op >();
            return *this;
        }

        template< Op op >
        _LART_INLINE constexpr Term apply_in_place( const Term& other ) noexcept
        {
            return this->extend( other ).apply< op >();
        }

        explicit operator void*() const { return pointer; }

        /* Constants are identified by having negative bitwidth. */
        template< typename T >
        _LART_INLINE static RPN::Constant< T > constant( T value ) noexcept
        {
            return { Op( -bitwidth< T >() ), value };
        }

        template< typename T >
        _LART_INLINE static Term lift_any( Abstracted< T > ) noexcept;

        /* Lift a constant to a Term. */
        template< typename T >
        _LART_INLINE static Term lift( T value ) noexcept
        {
            auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
            new ( ptr ) Base();
            Term term{ ptr };
            return term.extend( constant( value ) );
        }

        #define __lift( name, T ) \
            _LART_NOINLINE static Term lift_one_ ## name( T value ) noexcept { \
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
        static Term lift_one_ptr( void *p ) noexcept
        {
            return lift( reinterpret_cast< uintptr_t >( p ) );
        }

        _LART_INTERFACE
        static Tristate to_tristate( Term ) noexcept
        {
            return { Tristate::Unknown };
        }

        _LART_INLINE
        static constexpr bool faultable( Op op ) noexcept
        {
            using namespace brick::smt;
            return is_one_of< Op::BvUDiv
                            , Op::BvSDiv
                            , Op::BvURem
                            , Op::BvSRem
                            , Op::FpDiv
                            , Op::FpRem >( op );
        }

        template< Op op >
        _LART_INLINE static Term impl_binary( Term lhs, Term rhs ) noexcept
        {
            // resulting rpn: [ lhs | rhs | op ]

            auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
            new ( ptr ) Base();
            Term term{ ptr };

            return term.extend( lhs ).extend( rhs ).apply< op >();
        }

        template< Op op >
        _LART_INLINE static Term fault_i_bin( Term lhs, Term rhs ) noexcept
        {
            auto eq = op_eq( op_zext( rhs, 64 ), lift_one_i64( 0 ) );
            if ( to_tristate( eq ) ) {
                assume( eq, eq, true );
                fault_idiv_by_zero();
                return rhs; // return zero
            }

            assume( eq, eq, false );
            return impl_binary< op >( lhs, rhs );
        }


        template< Op op >
        _LART_INLINE static Term fault_f_bin( Term lhs, Term rhs ) noexcept
        {
            assert( false ); // not implemented
        }

        template< Op op >
        _LART_INLINE static Term binary( Term lhs, Term rhs ) noexcept
        {
            using namespace brick::smt;

            // resulting rpn: [ lhs | rhs | op ]

            if constexpr ( faultable( op ) ) {
                static_assert( is_float_bin( op ) || is_integer_bin( op ) );
                if constexpr ( is_integer_bin( op ) ) {
                    return fault_i_bin< op >( lhs, rhs );
                }
                if constexpr ( is_float_bin( op ) ) {
                    return fault_f_bin< op >( lhs, rhs );
                }
            } else {
                return impl_binary< op >( lhs, rhs );
            }
        }

        template< Op op >
        _LART_INLINE static Term unary( Term arg ) noexcept
        {
            // resulting rpn: [ arg | op ]
            assert( false );
        }

        template< Op op >
        _LART_INLINE static Term cmp( Term lhs, Term rhs ) noexcept
        {
            return Term::binary< op >( lhs, rhs );
        }

        template< Op op >
        _LART_INLINE static RPN::CastOp cast_op( Bitwidth bw ) noexcept
        {
            return { op, bw };
        }

        template< Op op >
        _LART_INLINE static Term cast( Term arg, Bitwidth bw ) noexcept
        {
            // resulting rpn: [ arg | bw | op ]
            auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
            new ( ptr ) Base();
            Term term{ ptr };

            return term.extend( arg ).extend( cast_op< op >( bw ) );
        }

        _LART_INTERFACE
        Term constrain( const Term &constraint, bool expect ) const noexcept;

        _LART_INTERFACE
        static Term assume( Term value, Term constraint, bool expect ) noexcept
        {
            return value.constrain( constraint, expect );
        }

        _LART_INTERFACE
        static Term op_thaw( Term term, uint8_t bw ) noexcept
        {
            return cast< Op::ZFit >( term, bw ); /* TODO interval-based peek & poke */
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
        __cast( op_fpext, FPExt );
        __cast( op_fptosi, FPToSInt );
        __cast( op_fptoui, FPToUInt );
        __cast( op_fptrunc, FPTrunc );
        //__cast( op_inttoptr );
        //__cast( op_ptrtoint );
        __cast( op_sext, SExt );
        __cast( op_sitofp, SIntToFP );
        __cast( op_trunc, Trunc );
        __cast( op_uitofp, UIntToFP );
        __cast( op_zext, ZExt );

        #undef __un
        #undef __bin
        #undef __cmp
        #undef __cast
        #undef __lift
    };

    /* `counter` is for unique variable names */
    struct TermState
    {
        uint16_t counter = 0;  // TODO: why is this thing not atomic
        Term constraints;
    };

    extern TermState __term_state;

    template< typename T >
    _LART_INTERFACE RPN::Variable variable() noexcept
    {
        return { RPN::var< T >(), __term_state.counter++ };
    }

    template< typename T >
    _LART_INTERFACE Term Term::lift_any( Abstracted< T > ) noexcept
    {
        auto ptr = __vm_obj_make( sizeof( BaseID ), _VM_PT_Marked );
        new ( ptr ) Base();
        Term term{ ptr };
        return term.extend( variable< T >() );
    }

    static_assert( sizeof( Term ) == 8 );

} // namespace abstract
