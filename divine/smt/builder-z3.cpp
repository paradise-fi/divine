// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
 * (c) 2017-2018 Petr Ročkai <code@fixp.eu>
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
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

#include <divine/smt/builder-z3.hpp>
#if OPT_Z3
namespace divine::smt::builder
{

using namespace std::literals;

z3::expr Z3::constant( bool v )
{
    return _ctx.bool_val( v );
}

z3::expr Z3::constant( uint64_t value, int bw )
{
    return _ctx.bv_val( value, bw );
}

z3::expr Z3::variable( int id, int bw )
{
    return _ctx.bv_const( ( "var_"s + std::to_string( id ) ).c_str(), bw );
}

z3::expr Z3::unary( brq::smt_op op, Node arg, int bw )
{
    int childbw = arg.is_bv() ? arg.get_sort().bv_size() : 1;

    if ( op == op_t::bv_zfit )
        op = bw > childbw ? op_t::bv_zext : op_t::bv_trunc;

    switch ( op )
    {
        case op_t::bv_trunc:
            ASSERT_LEQ( bw, childbw );
            ASSERT( arg.is_bv() );
            if ( bw == childbw )
                return arg;
            return arg.extract( bw - 1, 0 );
        case op_t::bv_zext:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return arg.is_bv()
                ? z3::zext( arg, bw - childbw )
                : z3::ite( arg, _ctx.bv_val( 1, bw ), _ctx.bv_val( 0, bw ) );
        case op_t::bv_sext:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return arg.is_bv()
                ? z3::sext( arg, bw - childbw )
                : z3::ite( arg, ~_ctx.bv_val( 0, bw ), _ctx.bv_val( 0, bw ) );

        /*case op_t::fp_ext:
            ASSERT_LT( childbw, bw );
            // Z3_mk_fpa_to_fp_float( arg.ctx() ); // TODO
            UNREACHABLE_F( "Unsupported operation." );
        case op_t::fp_trunc:
            ASSERT_LT( bw, childbw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case op_t::fp_tosbv:
            UNREACHABLE_F( "Unsupported operation." ); // TODO
            ASSERT_EQ( childbw, bw );
        case op_t::fp_toubv:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case op_t::bv_stofp:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case op_t::bv_utofp:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO*/
        case op_t::bv_not:
        case op_t::bool_not:
            ASSERT_EQ( childbw, bw );
            ASSERT_EQ( bw, 1 );
            return arg.is_bv() ? ~arg : !arg;
        default:
            UNREACHABLE( "unknown unary operation", op );
    }
}

z3::expr Z3::extract( Node arg, std::pair< int, int > bounds )
{
    ASSERT( arg.is_bv() );
    return arg.extract( bounds.first, bounds.second );
}

z3::expr Z3::binary( brq::smt_op op, Node a, Node b, int )
{
    if ( a.is_bv() && b.is_bv() )
    {
        switch ( op )
        {
            case op_t::bv_add:  return a + b;
            case op_t::bv_sub:  return a - b;
            case op_t::bv_mul:  return a * b;
            case op_t::bv_sdiv: return a / b;
            case op_t::bv_udiv: return z3::udiv( a, b );
            case op_t::bv_srem: return z3::srem( a, b );
            case op_t::bv_urem: return z3::urem( a, b );
            case op_t::bv_shl:  return z3::shl( a, b );
            case op_t::bv_ashr: return z3::ashr( a, b );
            case op_t::bv_lshr: return z3::lshr( a, b );
            case op_t::bool_and:
            case op_t::bv_and:  return a & b;
            case op_t::bv_or:   return a | b;
            case op_t::bv_xor:  return a ^ b;

            case op_t::bv_ule:  return z3::ule( a, b );
            case op_t::bv_ult:  return z3::ult( a, b );
            case op_t::bv_uge:  return z3::uge( a, b );
            case op_t::bv_ugt:  return z3::ugt( a, b );
            case op_t::bv_sle:  return a <= b;
            case op_t::bv_slt:  return a < b;
            case op_t::bv_sge:  return a >= b;
            case op_t::bv_sgt:  return a > b;
            case op_t::eq:   return a == b;
            case op_t::neq:   return a != b;
            case op_t::bv_concat: return z3::concat( a, b );
            default:
                UNREACHABLE( "unknown binary operation", op );
        }
    }
    /*else if ( a.is_real() && b.is_real() )
    {
        switch ( bin.op )
        {
            case op_t::bv_fpadd: return a + b;
            case op_t::bv_fpsub: return a - b;
            case op_t::bv_fpmul: return a * b;
            case op_t::bv_fpdiv: return a / b;
            case op_t::bv_fprem:
                UNREACHABLE_F( "unsupported operation FpFrem" );
            case op_t::bv_fpfalse:
                UNREACHABLE_F( "unsupported operation FpFalse" );
            case op_t::bv_fpoeq: return a == b;
            case op_t::bv_fpogt: return a > b;
            case op_t::bv_fpoge: return a >= b;
            case op_t::bv_fpolt: return a < b;
            case op_t::bv_fpole: return a <= b;
            case op_t::bv_fpone: return a != b;
            case op_t::bv_fpord:
                UNREACHABLE_F( "unsupported operation FpORD" );
            case op_t::bv_fpueq: return a == b;
            case op_t::bv_fpugt: return a > b;
            case op_t::bv_fpuge: return a >= b;
            case op_t::bv_fpult: return a < b;
            case op_t::bv_fpule: return a <= b;
            case op_t::bv_fpune: return a != b;
            case op_t::bv_fpuno:
                UNREACHABLE_F( "unknown binary operation FpUNO" );
            case op_t::bv_fptrue:
                UNREACHABLE_F( "unknown binary operation FpTrue" );
            default:
                unreachable_f( "unknown binary operation %d %d", bin.op, int( op_t::bv_eq ) );
        }
    }*/
    else
    {
        if ( a.is_bv() )
        {
            ASSERT_EQ( a.get_sort().bv_size(), 1 );
            a = ( a == _ctx.bv_val( 1, 1 ) );
        }

        if ( b.is_bv() )
        {
            ASSERT_EQ( b.get_sort().bv_size(), 1 );
            b = ( b == _ctx.bv_val( 1, 1 ) );
        }

        switch ( op )
        {
            case op_t::bool_xor:
            case op_t::bv_xor:
            case op_t::bv_sub:
                return a != b;
            case op_t::bv_add:
                return a && b;
            case op_t::bv_udiv:
            case op_t::bv_sdiv:
                return a;
            case op_t::bv_urem:
            case op_t::bv_srem:
            case op_t::bv_shl:
            case op_t::bv_lshr:
                return constant( false );
            case op_t::bv_ashr:
                return a;
            case op_t::bool_or:
            case op_t::bv_or:
                return a || b;
            case op_t::bool_and:
            case op_t::constraint:
            case op_t::bv_and:
                return a && b;
            case op_t::bv_uge:
            case op_t::bv_sle:
                return a || !b;
            case op_t::bv_ule:
            case op_t::bv_sge:
                return b || !a;
            case op_t::bv_ugt:
            case op_t::bv_slt:
                return a && !b;
            case op_t::bv_ult:
            case op_t::bv_sgt:
                return b && !a;
            case op_t::eq:
                return a == b;
            case op_t::neq:
                return a != b;
            default:
                UNREACHABLE( "unknown boolean binary operation", op );
        }
    }
}


} // namespace divine::smt::builder
#endif
