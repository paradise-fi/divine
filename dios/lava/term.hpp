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
        using base = brq::smt_expr< tagged_array_adaptor >;

        term() = default;
        term( void *v, __dios::construct_shared_t s ) : base( v, s ) {}

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
            constexpr auto op = brq::smt_match_op< brq::smt_op_var, bitwidth_v< T > >;
            return { brq::smt_atom_t< brq::smt_varid_t >( op, counter() ) };
        }

        template< typename T >
        static term lift( T value )
        {
            if constexpr ( std::is_same_v< T, array_ref > )
                NOT_IMPLEMENTED();
            else
            {
                constexpr auto op = brq::smt_match_op< brq::smt_op_const, bitwidth_v< T > >;
                return { brq::smt_atom_t< T >( op, value ) };
            }
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

            auto eq = op_eq( op_zext( divisor, 64 ), lift( 0ul ) );

            if ( to_tristate( eq ) )
            {
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
        using tt = const term &;
        using bw = uint8_t;

        static term cast( term arg, bitwidth_t bw, op o )
        {
            brq::smt_atom_t< uint8_t > atom( o, bw );
            return { arg, atom };
        }

        static term assume( tt value, tt constraint, bool expect );
        static term op_thaw( tt term, uint8_t bw )
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

        static term op_add ( tt a, tt b ) { return { a, b, op::bv_add  }; }
        static term op_sub ( tt a, tt b ) { return { a, b, op::bv_sub  }; }
        static term op_mul ( tt a, tt b ) { return { a, b, op::bv_mul  }; }
        static term op_sdiv( tt a, tt b ) { return div( a, b, op::bv_sdiv ); }
        static term op_udiv( tt a, tt b ) { return div( a, b, op::bv_udiv ); }
        static term op_srem( tt a, tt b ) { return div( a, b, op::bv_srem ); }
        static term op_urem( tt a, tt b ) { return div( a, b, op::bv_urem ); }

        static term op_fadd( tt a, tt b ) { return { a, b, op::fp_add }; }
        static term op_fsub( tt a, tt b ) { return { a, b, op::fp_sub }; }
        static term op_fmul( tt a, tt b ) { return { a, b, op::fp_mul }; }
        static term op_fdiv( tt a, tt b ) { return div( a, b, op::fp_div ); }
        static term op_frem( tt a, tt b ) { return div( a, b, op::fp_rem ); }

        static term op_shl ( tt a, tt b ) { return { a, b, op::bv_shl  }; }
        static term op_ashr( tt a, tt b ) { return { a, b, op::bv_ashr }; }
        static term op_lshr( tt a, tt b ) { return { a, b, op::bv_lshr }; }
        static term op_and ( tt a, tt b ) { return { a, b, op::bv_and  }; }
        static term op_or  ( tt a, tt b ) { return { a, b, op::bv_or   }; }
        static term op_xor ( tt a, tt b ) { return { a, b, op::bv_xor  }; }

        static term op_eq ( tt a, tt b ) { return { a, b, op::eq }; };
        static term op_ne ( tt a, tt b ) { return { a, b, op::neq }; };
        static term op_ugt( tt a, tt b ) { return { a, b, op::bv_ugt }; };
        static term op_uge( tt a, tt b ) { return { a, b, op::bv_uge }; };
        static term op_ult( tt a, tt b ) { return { a, b, op::bv_ult }; };
        static term op_ule( tt a, tt b ) { return { a, b, op::bv_ule }; };
        static term op_sgt( tt a, tt b ) { return { a, b, op::bv_sgt }; };
        static term op_sge( tt a, tt b ) { return { a, b, op::bv_sge }; };
        static term op_slt( tt a, tt b ) { return { a, b, op::bv_slt }; };
        static term op_sle( tt a, tt b ) { return { a, b, op::bv_sle }; };

        static term op_foeq( tt a, tt b ) { return { a, b, op::fp_oeq }; }
        static term op_fogt( tt a, tt b ) { return { a, b, op::fp_ogt }; }
        static term op_foge( tt a, tt b ) { return { a, b, op::fp_oge }; }
        static term op_folt( tt a, tt b ) { return { a, b, op::fp_olt }; }
        static term op_fole( tt a, tt b ) { return { a, b, op::fp_ole }; }
        static term op_ford( tt a, tt b ) { return { a, b, op::fp_ord }; }
        static term op_funo( tt a, tt b ) { return { a, b, op::fp_uno }; }
        static term op_fueq( tt a, tt b ) { return { a, b, op::fp_ueq }; }
        static term op_fugt( tt a, tt b ) { return { a, b, op::fp_ugt }; }
        static term op_fuge( tt a, tt b ) { return { a, b, op::fp_uge }; }
        static term op_fult( tt a, tt b ) { return { a, b, op::fp_ult }; }
        static term op_fule( tt a, tt b ) { return { a, b, op::fp_ule }; }

        static term op_ffalse( tt a, tt b ) { return { a, b, op::fp_false }; }
        static term op_ftrue( tt a, tt b ) { return { a, b, op::fp_true }; }

        static term op_trunc  ( tt a, bw w ) { return cast( a, w, op::bv_trunc ); }
        static term op_fptrunc( tt a, bw w ) { return cast( a, w, op::fp_trunc ); }
        static term op_sitofp ( tt a, bw w ) { return cast( a, w, op::bv_stofp ); }
        static term op_uitofp ( tt a, bw w ) { return cast( a, w, op::bv_utofp ); }
        static term op_zext   ( tt a, bw w ) { return cast( a, w, op::bv_zext ); }
        static term op_zfit   ( tt a, bw w ) { return cast( a, w, op::bv_zfit ); }
        static term op_sext   ( tt a, bw w ) { return cast( a, w, op::bv_sext ); }
        static term op_fpext  ( tt a, bw w ) { return cast( a, w, op::fp_ext ); }
        static term op_fptosi ( tt a, bw w ) { return cast( a, w, op::fp_tosbv ); }
        static term op_fptoui ( tt a, bw w ) { return cast( a, w, op::fp_toubv ); }

        static term op_concat ( tt a, tt b ) { return { a, b, op::bv_concat }; }

        // op_inttoptr
        // op_ptrtoint
    };

    struct term_state_t
    {
        template< typename val >
        using dense_map = brq::dense_map< brq::smt_varid_t, val, __dios::Array< val, _VM_PT_Weak > >;
        brq::union_find< dense_map< brq::smt_varid_t > > uf;
        __dios::ArrayMap< brq::smt_varid_t, term > decomp; /* maps representants to terms */
    };

    extern term_state_t *__term_state;
    static_assert( sizeof( term ) == 8 );
}

extern "C" void *__dios_term_init();
