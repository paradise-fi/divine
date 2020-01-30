// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018-2019 Henrich Lauko <xlauko@mail.muni.cz>
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

#include <divine/smt/builder-smtlib.hpp>

namespace divine::smt::builder
{

using namespace std::literals;

SMTLib2::Node SMTLib2::define( Node def )
{
    if ( !_use_defs )
        return def;
    auto name = "def_"s + std::to_string( _def_counter ++ ) + _suff;
    return _ctx.define( name, def );
}

SMTLib2::Node SMTLib2::variable( int id, int bw )
{
    auto name = "var_" + std::to_string( id );
    return _ctx.variable( _ctx.bitvecT( bw ), name );
}

SMTLib2::Node SMTLib2::constant( bool v )
{
    return v ? _ctx.symbol( 1, Node::t_bool, "true" ) : _ctx.symbol( 1, Node::t_bool, "false" );
}

SMTLib2::Node SMTLib2::constant( uint64_t value, int bw )
{
    return _ctx.bitvec( bw, value );
}

SMTLib2::Node SMTLib2::unary( brq::smt_op op, Node arg, int bw )
{
    if ( op == op_t::bv_zfit )
        op = bw > arg.bw ? op_t::bv_zext : op_t::bv_trunc;

    switch ( op )
    {
        case op_t::bv_trunc:
        {
            ASSERT_LEQ( bw, arg.bw );
            if ( arg.bw == bw )
                return arg;
            auto op = _ctx.extract( bw - 1, 0, arg );
            if ( bw == 1 )
                op = _ctx.binop< op_t::eq >( bw, op, _ctx.bitvec( 1, 1 ) );
            return define( op );
        }

        case op_t::bv_zext:
            ASSERT_LEQ( arg.bw, bw );
            if ( arg.bw == bw )
                return arg;
            return define( arg.bw > 1
                           ? _ctx.binop< op_t::bv_concat >( bw, _ctx.bitvec( bw - arg.bw, 0 ), arg )
                           : _ctx.ite( arg, _ctx.bitvec( bw, 1 ), _ctx.bitvec( bw, 0 ) ) );

        case op_t::bv_sext:
            ASSERT_LEQ( arg.bw, bw );
            if ( arg.bw == bw )
                return arg;
            if ( arg.bw == 1 )
            {
                auto ones = _ctx.unop< op_t::bv_not >( bw, _ctx.bitvec( bw, 0 ) ),
                   zeroes = _ctx.bitvec( bw, 0 );
                return define( _ctx.ite( arg, ones, zeroes ) );
            }
            else
            {
                int extbw = bw - arg.bw;
                auto one_ext = _ctx.bitvec( extbw, brick::bitlevel::ones< uint64_t >( extbw ) ),
                       z_ext = _ctx.bitvec( extbw, 0 ),
                        sign = _ctx.binop< op_t::eq >( 1, _ctx.bitvec( 1, 1 ),
                                                          _ctx.extract( arg.bw - 1, arg.bw - 1, arg ) );
                return define( _ctx.binop< op_t::bv_concat >( bw, _ctx.ite( sign, one_ext, z_ext ),
                                                              arg ) );
            }

        case op_t::fp_ext:
        case op_t::fp_trunc:
        case op_t::fp_tosbv:
        case op_t::fp_toubv:
        case op_t::bv_stofp:
        case op_t::bv_utofp:
            return _ctx.cast( op, bw, arg );

        case op_t::bv_not:
        case op_t::bool_not:
            ASSERT_EQ( arg.bw, bw );
            ASSERT_EQ( bw, 1 );
            return define( arg.is_bool() ? _ctx.unop< op_t::bool_not >( 1, arg )
                                         : _ctx.unop< op_t::bv_not >( 1, arg ) );

        default:
            UNREACHABLE( "unknown unary operation", op );
    }
}

SMTLib2::Node SMTLib2::extract( Node arg, std::pair< int, int > bounds )
{
    return define( _ctx.extract( bounds.second, bounds.first, arg ) );
}

SMTLib2::Node SMTLib2::binary( brq::smt_op op, Node a, Node b, int bw )
{
    if ( a.is_bv() && b.is_bv() )
    {
        switch ( op )
        {
            case op_t::bv_add:
            case op_t::bv_sub:
            case op_t::bv_mul:
            case op_t::bv_sdiv:
            case op_t::bv_udiv:
            case op_t::bv_srem:
            case op_t::bv_urem:
            case op_t::bv_shl:
            case op_t::bv_ashr:
            case op_t::bv_lshr:
            case op_t::bv_and:
            case op_t::bv_or:
            case op_t::bv_xor:
                ASSERT_EQ( a.bw, b.bw );
                return define( _ctx.expr( bw, op, { a, b } ) );

            case op_t::bv_ule:
            case op_t::bv_ult:
            case op_t::bv_uge:
            case op_t::bv_ugt:
            case op_t::bv_sle:
            case op_t::bv_slt:
            case op_t::bv_sge:
            case op_t::bv_sgt:
            case op_t::eq:
                ASSERT_EQ( bw, 1, op );
                return define( _ctx.expr( bw, op, { a, b } ) );

            case op_t::neq:
                ASSERT_EQ( bw, 1 );
                return define( _ctx.unop< op_t::bool_not >( bw, _ctx.binop< op_t::eq >( bw, a, b ) ) );

            case op_t::bv_concat:
                ASSERT_EQ( bw, a.bw + b.bw );
                return define( _ctx.binop< op_t::bv_concat >( bw, a, b ) );

            default:
                UNREACHABLE( "unknown binary operation:", a, op, b, _ctx.defs );
        }
    }
    else if ( a.is_float() && b.is_float() )
    {
        Node node = a;

        switch ( op )
        {
            case op_t::fp_add:
            case op_t::fp_sub:
            case op_t::fp_mul:
            case op_t::fp_div:
            case op_t::fp_rem:
                return define( _ctx.expr( bw, op, { a, b }, brq::smtlib_rounding::RNE ) );

            case op_t::fp_false:
            case op_t::fp_true:
            case op_t::fp_oeq:
            case op_t::fp_ogt:
            case op_t::fp_oge:
            case op_t::fp_olt:
            case op_t::fp_ole:
            // case op_t::fp_one:
            case op_t::fp_ord:
            case op_t::fp_ueq:
            case op_t::fp_ugt:
            case op_t::fp_uge:
            case op_t::fp_ult:
            case op_t::fp_ule:
            // case op_t::fp_une:
            case op_t::fp_uno:
                ASSERT_EQ( bw, 1 );
                return define( _ctx.expr( bw, op, { a, b }, brq::smtlib_rounding::RNE ) );

            default:
                UNREACHABLE( "unknown binary operation", op );
        }
    }
    else
    {
        ASSERT( !a.is_float() );
        ASSERT( !b.is_float() );

        if ( a.is_bv() )
        {
            ASSERT_EQ( a.bw, 1 );
            a = define( _ctx.binop< op_t::eq >( 1, a, _ctx.bitvec( 1, 1 ) ) );
        }

        if ( b.is_bv() )
        {
            ASSERT_EQ( b.bw, 1 );
            b = define( _ctx.binop< op_t::eq >( 1, b, _ctx.bitvec( 1, 1 ) ) );
        }

        switch ( op )
        {
            case op_t::bool_xor:
            case op_t::bv_sub:
            case op_t::bv_xor:
                return define( _ctx.binop< op_t::bool_xor >( 1, a, b ) );
            case op_t::bv_add:
                return define( _ctx.binop< op_t::bool_and >( 1, a, b ) );
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
                return define( _ctx.binop< op_t::bool_or >( 1, a, b ) );
            case op_t::bool_and:
            case op_t::bv_and:
                return define( _ctx.binop< op_t::bool_and >( 1, a, b ) );
            case op_t::bv_uge:
            case op_t::bv_sle:
                return define( _ctx.binop< op_t::bool_or >( 1, a, _ctx.unop< op_t::bool_not >( 1, b ) ) );
            case op_t::bv_ule:
            case op_t::bv_sge:
                return define( _ctx.binop< op_t::bool_or >( 1, b, _ctx.unop< op_t::bool_not >( 1, a ) ) );
            case op_t::bv_ugt:
            case op_t::bv_slt:
                return define( _ctx.binop< op_t::bool_and >( 1, a, _ctx.unop< op_t::bool_not >( 1, b ) ) );
            case op_t::bv_ult:
            case op_t::bv_sgt:
                return define( _ctx.binop< op_t::bool_and >( 1, b, _ctx.unop< op_t::bool_not >( 1, a ) ) );
            case op_t::eq:
                return define( _ctx.binop< op_t::eq >( 1, a, b ) );
            case op_t::neq:
                return define( _ctx.unop< op_t::bool_not >( 1, _ctx.binop< op_t::bool_and >( 1, a, b ) ) );
            default:
                UNREACHABLE( "unknown boolean binary operation", op );
        }
    }
    UNREACHABLE( "unexpected operands", a, b, "to", op );
}

} // namespace divine::smt::builder
