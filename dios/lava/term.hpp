// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019, 2020 Petr Ročkai <code@fixp.eu>
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "base.hpp"
#include "tristate.hpp"
#include <util/map.hpp>

#include <brick-nusmt>
#include <type_traits>

namespace __lava
{
    template< typename >
    using tagged_array_adaptor = tagged_array< _VM_PT_Marked >;

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

    struct term : brq::smt_expr< tagged_array_adaptor >, domain_mixin< term >
    {
        static constexpr struct {} tag;
        using base = brq::smt_expr< tagged_array_adaptor >;

        template< typename... args_t >
        term( const args_t & ... args )
            : brq::smt_expr< tagged_array_adaptor >()
        {
            apply( args... );
        }

        static int counter()
        {
            static int v = 0;
            return ++v;
        }

        template< typename T >
        static term any()
        {
            constexpr auto op = brq::smt_match_op< brq::smt_op_var, sizeof( T ) * 8 >;
            return { brq::smt_atom_t< T >( op, counter() ) };
        }

        template< typename T >
        static term lift( T value )
        {
            constexpr auto op = brq::smt_match_op< brq::smt_op_const, sizeof( T ) * 8 >;
            return { brq::smt_atom_t< T >( op, value ) };
        }

        static tristate to_tristate( term ) { return { tristate::maybe }; }

        static term div( term a, term b, brq::smt_op o )
        {
            auto fault = _VM_Fault::_VM_F_Integer;
            auto divisor = b;

            if ( o == op::fp_div || o == op::fp_rem )
            {
                divisor = op_fptosi( b, 64 );
                fault = _VM_Fault::_VM_F_Float;
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
                __dios_fault( fault, "division by zero" );
                return b;
            }
            else
            {
                assume( eq, eq, false );
                return { a, b, o };
            }
        }

        using op = brq::smt_op;
        using tt = term;
        using bw = uint8_t;

        static term cast( term arg, bitwidth_t bw, op o )
        {
            brq::smt_atom_t< uint8_t > atom( o, bw );
            return { arg, atom };
        }

        static tt assume( tt value, tt constraint, bool expect );
        static tt op_thaw( tt term, uint8_t bw )
        {
            /* TODO interval-based peek & poke */
            return cast( term, bw, op::bv_zfit );
        }

        static term op_extract( term t, uint8_t from, uint8_t to )
        {
            std::pair arg{ from, to };
            brq::smt_atom_t< std::pair< uint8_t, uint8_t > > atom( op::bv_extract, arg );
            return { t, atom };
        }

        static tt op_add ( tt a, tt b ) { return { a, b, op::bv_add  }; }
        static tt op_sub ( tt a, tt b ) { return { a, b, op::bv_sub  }; }
        static tt op_mul ( tt a, tt b ) { return { a, b, op::bv_mul  }; }
        static tt op_sdiv( tt a, tt b ) { return div( a, b, op::bv_sdiv ); }
        static tt op_udiv( tt a, tt b ) { return div( a, b, op::bv_udiv ); }
        static tt op_srem( tt a, tt b ) { return div( a, b, op::bv_srem ); }
        static tt op_urem( tt a, tt b ) { return div( a, b, op::bv_urem ); }

        static tt op_fadd( tt a, tt b ) { return { a, b, op::fp_add }; }
        static tt op_fsub( tt a, tt b ) { return { a, b, op::fp_sub }; }
        static tt op_fmul( tt a, tt b ) { return { a, b, op::fp_mul }; }
        static tt op_fdiv( tt a, tt b ) { return div( a, b, op::fp_div ); }
        static tt op_frem( tt a, tt b ) { return div( a, b, op::fp_rem ); }

        static tt op_shl ( tt a, tt b ) { return { a, b, op::bv_shl  }; }
        static tt op_ashr( tt a, tt b ) { return { a, b, op::bv_ashr }; }
        static tt op_lshr( tt a, tt b ) { return { a, b, op::bv_lshr }; }
        static tt op_and ( tt a, tt b ) { return { a, b, op::bv_and  }; }
        static tt op_or  ( tt a, tt b ) { return { a, b, op::bv_or   }; }
        static tt op_xor ( tt a, tt b ) { return { a, b, op::bv_xor  }; }

        static tt op_eq ( tt a, tt b ) { return { a, b, op::eq }; };
        static tt op_neq( tt a, tt b ) { return { a, b, op::neq }; };
        static tt op_ugt( tt a, tt b ) { return { a, b, op::bv_ugt }; };
        static tt op_uge( tt a, tt b ) { return { a, b, op::bv_uge }; };
        static tt op_ult( tt a, tt b ) { return { a, b, op::bv_ult }; };
        static tt op_ule( tt a, tt b ) { return { a, b, op::bv_ule }; };
        static tt op_sgt( tt a, tt b ) { return { a, b, op::bv_sgt }; };
        static tt op_sge( tt a, tt b ) { return { a, b, op::bv_sge }; };
        static tt op_slt( tt a, tt b ) { return { a, b, op::bv_slt }; };
        static tt op_sle( tt a, tt b ) { return { a, b, op::bv_sle }; };

        static tt op_foeq( tt a, tt b ) { return { a, b, op::fp_oeq }; }
        static tt op_fogt( tt a, tt b ) { return { a, b, op::fp_ogt }; }
        static tt op_foge( tt a, tt b ) { return { a, b, op::fp_oge }; }
        static tt op_folt( tt a, tt b ) { return { a, b, op::fp_olt }; }
        static tt op_fole( tt a, tt b ) { return { a, b, op::fp_ole }; }
        static tt op_ford( tt a, tt b ) { return { a, b, op::fp_ord }; }
        static tt op_funo( tt a, tt b ) { return { a, b, op::fp_uno }; }
        static tt op_fueq( tt a, tt b ) { return { a, b, op::fp_ueq }; }
        static tt op_fugt( tt a, tt b ) { return { a, b, op::fp_ugt }; }
        static tt op_fuge( tt a, tt b ) { return { a, b, op::fp_uge }; }
        static tt op_fult( tt a, tt b ) { return { a, b, op::fp_ult }; }
        static tt op_fule( tt a, tt b ) { return { a, b, op::fp_ule }; }

        static tt op_ffalse( tt a, tt b ) { return { a, b, op::fp_false }; }
        static tt op_ftrue( tt a, tt b ) { return { a, b, op::fp_true }; }

        static tt op_trunc  ( tt a, bw w ) { return cast( a, w, op::bv_trunc ); }
        static tt op_fptrunc( tt a, bw w ) { return cast( a, w, op::fp_trunc ); }
        static tt op_sitofp ( tt a, bw w ) { return cast( a, w, op::bv_stofp ); }
        static tt op_uitofp ( tt a, bw w ) { return cast( a, w, op::bv_utofp ); }
        static tt op_zext   ( tt a, bw w ) { return cast( a, w, op::bv_zext ); }
        static tt op_sext   ( tt a, bw w ) { return cast( a, w, op::bv_sext ); }
        static tt op_fpext  ( tt a, bw w ) { return cast( a, w, op::fp_ext ); }
        static tt op_fptosi ( tt a, bw w ) { return cast( a, w, op::fp_tosbv ); }
        static tt op_fptoui ( tt a, bw w ) { return cast( a, w, op::fp_toubv ); }

        static tt op_concat ( tt a, tt b ) { return { a, b, op::bv_concat }; }

        // op_inttoptr
        // op_ptrtoint
    };

    struct term_state_t
    {
        template< typename val >
        using dense_map = brq::dense_map< brq::smt_varid_t, val, __dios::Array< val, _VM_PT_Weak > >;
        brq::union_find< dense_map< brq::smt_varid_t > > uf;
        dense_map< term > decomp; /* maps representants to terms */
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
