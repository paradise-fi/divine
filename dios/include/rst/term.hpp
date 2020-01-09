// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>
#include <rst/base.hpp>
#include <util/map.hpp>

#include <brick-smt>
#include <type_traits>

namespace __dios::rst::abstract
{
    namespace smt = brick::smt;
    using RPN = smt::RPN< Array< uint8_t, _VM_PT_Marked > >;

    /* Values in the term_t abstract domain are built out of Constants, Variables and
     * operations defined on them. Integers are represented as bitvectors of fixed
     * bitwidth, operations similarly have a bitwidth specified (of the arguments they
     * take). See `bricks/brick-smt` for details on the types and operations.
     *
     * A term_t is stored/encoded as bytecode in the `pointer` data member, which
     * is a pointer to an RPN structure (RPN is also defined in `brick-smt`).
     *
     * If method name is in form `op_X` then `X` should correspond to the llvm
     * instruction names. These methods are used in lart and their lookup is name-based
     * therefore it is mandatory to satisfy this requirement.
     * Otherwise there are no requirements. */
    struct term_t
    {
        void * pointer = nullptr;     // RPN * ?

        using Op = smt::Op;

        operator abstract_value_t() { return static_cast< abstract_value_t >( pointer ); }

        _LART_INLINE static term_t make_term() noexcept
        {
            auto ptr = __vm_obj_make( sizeof( base_id_t ), _VM_PT_Marked );
            new ( ptr ) tagged_abstract_domain_t();
            return term_t{ ptr };
        }

        template< typename ...Terms >
        _LART_INLINE static term_t make_term( Terms ...args ) noexcept
        {
            auto term = make_term();
            ( term.extend( args ), ... );
            return term;
        }

        _LART_INLINE RPN& as_rpn() noexcept
        {
            return *reinterpret_cast< RPN * >( &pointer );
        }

        _LART_INLINE const RPN& as_rpn() const noexcept
        {
            return *reinterpret_cast< const RPN * >( &pointer );
        }

        template< typename T >
        _LART_INLINE constexpr term_t extend( const T& value ) noexcept
        {
            this->as_rpn().extend( value );
            return *this;
        }

        _LART_INLINE constexpr term_t extend( const term_t& other ) noexcept
        {
            return extend( other.as_rpn() );
        }

        template< Op op >
        _LART_INLINE constexpr term_t apply() noexcept
        {
            this->as_rpn().apply< op >();
            return *this;
        }

        template< Op op >
        _LART_INLINE constexpr term_t apply_in_place( const term_t& other ) noexcept
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
        _LART_NOINLINE static term_t lift_any( Abstracted< T > ) noexcept;

        /* Lift a constant to a term_t. */
        template< typename T >
        _LART_NOINLINE static term_t lift( T value ) noexcept
        {
            auto ptr = __vm_obj_make( sizeof( base_id_t ), _VM_PT_Marked );
            new ( ptr ) tagged_abstract_domain_t();
            term_t term{ ptr };
            return term.extend( constant( value ) );
        }

        #define __lift( name, T ) \
            _LART_INTERFACE static term_t lift_one_ ## name( T value ) noexcept { \
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
        static term_t lift_any_aggr( unsigned size ) noexcept { return {}; }
        _LART_INTERFACE
        static term_t lift_one_aggr( void * aggr, unsigned size ) noexcept { return {}; }

        _LART_INTERFACE
        static term_t lift_one_ptr( void *p ) noexcept
        {
            return lift( reinterpret_cast< uintptr_t >( p ) );
        }

        _LART_INTERFACE
        static tristate_t to_tristate( term_t ) noexcept
        {
            return { tristate_t::Unknown };
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

        template< Op op, typename ...T >
        _LART_INLINE static term_t impl_nary( T ...terms )
        {
            auto ptr = __vm_obj_make( sizeof( base_id_t ), _VM_PT_Marked );
            new ( ptr ) tagged_abstract_domain_t();
            term_t term{ ptr };

            return apply_impl< op >( term, terms...);
        }

        template< Op op, typename H, typename ...T >
        _LART_INLINE static term_t apply_impl( term_t term, H h, T ...terms )
        {
            return apply_impl< op, T...>( term.extend( h ), terms... );
        }

        template< Op op >
        _LART_INLINE static term_t apply_impl( term_t term )
        {
            return term.apply< op >();
        }

        template< Op op >
        _LART_INLINE static term_t impl_binary( term_t lhs, term_t rhs ) noexcept
        {
            // resulting rpn: [ lhs | rhs | op ]

            auto ptr = __vm_obj_make( sizeof( base_id_t ), _VM_PT_Marked );
            new ( ptr ) tagged_abstract_domain_t();
            term_t term{ ptr };

            return term.extend( lhs ).extend( rhs ).apply< op >();
        }

        template< Op op >
        _LART_INLINE static term_t fault_i_bin( term_t lhs, term_t rhs ) noexcept
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
        _LART_INLINE static term_t fault_f_bin( term_t lhs, term_t rhs ) noexcept
        {
            assert( false ); // not implemented
        }

        template< Op op >
        _LART_INLINE static term_t binary( term_t lhs, term_t rhs ) noexcept
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
        _LART_INLINE static term_t unary( term_t arg ) noexcept
        {
            // resulting rpn: [ arg | op ]
            assert( false );
        }

        template< Op op >
        _LART_INLINE static term_t cmp( term_t lhs, term_t rhs ) noexcept
        {
            return term_t::binary< op >( lhs, rhs );
        }

        template< Op op >
        _LART_INLINE static RPN::CastOp cast_op( bitwidth_t bw ) noexcept
        {
            return { op, bw };
        }

        template< Op op >
        _LART_INLINE static term_t cast( term_t arg, bitwidth_t bw ) noexcept
        {
            // resulting rpn: [ arg | bw | op ]
            auto ptr = __vm_obj_make( sizeof( base_id_t ), _VM_PT_Marked );
            new ( ptr ) tagged_abstract_domain_t();
            term_t term{ ptr };

            return term.extend( arg ).extend( cast_op< op >( bw ) );
        }

        _LART_INTERFACE static term_t op_insertvalue( term_t arg, term_t value, uint64_t offset )
        {
            auto lhs = impl_nary< Op::Extract >(
                    arg,
                    constant( 0 ),
                    impl_binary< Op::BvSub >( lift( offset ), lift( 1 ) ) );
            auto rhs = impl_nary< Op::Extract >(
                    arg,
                    impl_binary< Op::BvAdd >( lift( offset ), lift( 1 ) ) );
            return impl_nary< Op::Concat >(
                    impl_nary< Op::Concat >( lhs, value ),
                    rhs );
        }

        _LART_INTERFACE static term_t op_extractvalue( term_t arg, uint64_t offset )
        {
            return impl_nary< Op::Extract >(
                    arg,
                    offset,
                    impl_binary< Op::BvAdd >( lift( offset ), lift( 1 ) ) );
        }

        _LART_INTERFACE
        term_t constrain( term_t &constraint, bool expect ) const noexcept;

        _LART_INTERFACE
        static term_t assume( term_t value, term_t constraint, bool expect ) noexcept
        {
            return value.constrain( constraint, expect );
        }

        _LART_INTERFACE
        static term_t op_thaw( term_t term, uint8_t bw ) noexcept
        {
            return cast< Op::ZFit >( term, bw ); /* TODO interval-based peek & poke */
        }

        _LART_INTERFACE
        static term_t op_extract( term_t term, uint8_t from, uint8_t to ) noexcept
        {
            return make_term( term ).apply< Op::Extract >().extend( from ).extend( to );
        }

        #define __bin( name, op ) \
            _LART_INTERFACE static term_t name( term_t lhs, term_t rhs ) noexcept { \
                return binary< Op::op >( lhs, rhs ); \
            }

        #define __un( name, op ) \
            _LART_INTERFACE static term_t name( term_t arg ) noexcept { \
                return unary< Op::op >( arg ); \
            }

        #define __cmp( name, op ) \
            _LART_INTERFACE static term_t name( term_t lhs, term_t rhs ) noexcept { \
                return cmp< Op::op >( lhs, rhs ); \
            }

        #define __cast( name, op ) \
            _LART_INTERFACE static term_t name( term_t arg, bitwidth_t bw ) noexcept { \
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
    struct term_state_t
    {
        using var_id_t = smt::token::VarID;

        uint16_t counter = 0;
        term_t constraints; // TODO: remove

        smt::union_find< ArrayMap< var_id_t, var_id_t, _VM_PT_Weak > > uf;
        ArrayMap< var_id_t, term_t, _VM_PT_Weak > decomp; // union-find representant to relevant RPNs
    };

    extern term_state_t *__term_state;

    template< typename T >
    _LART_INTERFACE RPN::Variable variable() noexcept
    {
        return { RPN::var< T >(), __term_state->counter++ };
    }

    template< typename T >
    _LART_INTERFACE term_t term_t::lift_any( Abstracted< T > ) noexcept
    {
        auto ptr = __vm_obj_make( sizeof( base_id_t ), _VM_PT_Marked );
        new ( ptr ) tagged_abstract_domain_t();
        term_t term{ ptr };
        return term.extend( variable< T >() );
    }

    static_assert( sizeof( term_t ) == 8 );
}

extern "C" void *__dios_term_init();
