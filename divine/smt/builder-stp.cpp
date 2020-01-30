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

#include <divine/smt/builder-stp.hpp>

#if OPT_STP

namespace divine::smt::builder
{

using namespace std::literals;

stp::ASTNode STP::constant( uint64_t value, int bw )
{
    return _stp.CreateBVConst( bw, value );
}

stp::ASTNode STP::constant( int value )
{
    return constant( value, 32 );
}

stp::ASTNode STP::constant( bool v )
{
    return v ? _stp.ASTTrue : _stp.ASTFalse;
}

stp::ASTNode STP::variable( int id, int bw )
{
    return _stp.CreateSymbol( ( "var_"s + std::to_string( id ) ).c_str(), 0, bw );
}

static bool is_bv( stp::ASTNode n ) { return n.GetType() == stp::BITVECTOR_TYPE; }

stp::ASTNode STP::unary( brq::smt_op op, Node arg, int bw )
{
    int childbw = is_bv( arg ) ? arg.GetValueWidth() : 1;

    if ( op == op_t::bv_zfit )
        op = bw > childbw ? op_t::bv_zext : op_t::bv_trunc;

    switch ( op )
    {
        case op_t::bv_trunc:
            ASSERT_LEQ( bw, childbw );
            ASSERT( is_bv( arg ) );
            if ( bw == childbw )
                return arg;
            return _stp.CreateTerm( stp::BVEXTRACT, bw, arg, constant( bw - 1 ), constant( 0 ) );
        case op_t::bv_zext:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return is_bv( arg )
                ? _stp.CreateTerm( stp::BVCONCAT, bw, constant( 0, bw - childbw ), arg )
                : _stp.CreateTerm( stp::ITE, bw, arg, constant( 1, bw ), constant( 0, bw ) );
        case op_t::bv_sext:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return is_bv( arg )
                ? _stp.CreateTerm( stp::BVSX, bw, arg, constant( bw ) )
                : _stp.CreateTerm( stp::ITE, bw, arg,
                                   constant( brick::bitlevel::ones< uint64_t >( bw ), bw ),
                                   constant( 0, bw ) );
        case op_t::bv_not:
        case op_t::bool_not:
            ASSERT_EQ( childbw, bw );
            ASSERT_EQ( bw, 1 );
            return is_bv( arg ) ? _stp.CreateTerm( stp::BVNOT, childbw, arg )
                                : _stp.CreateNode( stp::NOT, arg );
        default:
            UNREACHABLE( "unknown unary operation", op );
    }
}

stp::ASTNode STP::extract( Node arg, std::pair< int, int > bounds )
{
    int bw = bounds.second - bounds.first + 1;
    ASSERT( is_bv( arg ) );
    ASSERT_LT( bw, arg.GetValueWidth() );

    return _stp.CreateTerm( stp::BVEXTRACT, bw, arg,
                            constant( bounds.first ), constant( bounds.second ) );
}

stp::ASTNode STP::binary( brq::smt_op op, Node a, Node b, int bw )
{
    if ( is_bv( a ) && is_bv( b ) )
    {
        switch ( op )
        {
            case op_t::bv_add:    return _stp.CreateTerm( stp::BVPLUS,       bw, a, b );
            case op_t::bv_sub:    return _stp.CreateTerm( stp::BVSUB,        bw, a, b );
            case op_t::bv_mul:    return _stp.CreateTerm( stp::BVMULT,       bw, a, b );
            case op_t::bv_sdiv:   return _stp.CreateTerm( stp::SBVDIV,       bw, a, b );
            case op_t::bv_udiv:   return _stp.CreateTerm( stp::BVDIV,        bw, a, b );
            case op_t::bv_srem:   return _stp.CreateTerm( stp::SBVREM,       bw, a, b );
            case op_t::bv_urem:   return _stp.CreateTerm( stp::BVMOD,        bw, a, b );
            case op_t::bv_shl:    return _stp.CreateTerm( stp::BVLEFTSHIFT,  bw, a, b );
            case op_t::bv_ashr:   return _stp.CreateTerm( stp::BVSRSHIFT,    bw, a, b );
            case op_t::bv_lshr:   return _stp.CreateTerm( stp::BVRIGHTSHIFT, bw, a, b );
            case op_t::bool_and:
            case op_t::bv_and:    return _stp.CreateTerm( stp::BVAND,        bw, a, b );
            case op_t::bv_or:     return _stp.CreateTerm( stp::BVOR,         bw, a, b );
            case op_t::bv_xor:    return _stp.CreateTerm( stp::BVXOR,        bw, a, b );
            case op_t::bv_concat: return _stp.CreateTerm( stp::BVCONCAT,     bw, a, b );

            case op_t::bv_ule:    return _stp.CreateNode( stp::BVLE,  a, b );
            case op_t::bv_ult:    return _stp.CreateNode( stp::BVLT,  a, b );
            case op_t::bv_uge:    return _stp.CreateNode( stp::BVGE,  a, b );
            case op_t::bv_ugt:    return _stp.CreateNode( stp::BVGT,  a, b );
            case op_t::bv_sle:    return _stp.CreateNode( stp::BVSLE, a, b );
            case op_t::bv_slt:    return _stp.CreateNode( stp::BVSLT, a, b );
            case op_t::bv_sge:    return _stp.CreateNode( stp::BVSGE, a, b );
            case op_t::bv_sgt:    return _stp.CreateNode( stp::BVSGT, a, b );
            case op_t::eq:        return _stp.CreateNode( stp::EQ,    a, b );
            case op_t::neq:       return _stp.CreateNode( stp::NOT, _stp.CreateNode( stp::EQ, a, b ) );

            default:
                UNREACHABLE( "unknown binary operation", op );
        }
    }
    else
    {
        if ( is_bv( a ) )
        {
            ASSERT_EQ( a.GetValueWidth(), 1, op, a, b );
            a = _stp.CreateNode( stp::EQ, a, constant( 1, 1 ) );
        }

        if ( is_bv( b ) )
        {
            ASSERT_EQ( b.GetValueWidth(), 1 );
            b = _stp.CreateNode( stp::EQ, b, constant( 1, 1 ) );
        }

        switch ( op )
        {
            case op_t::bool_xor:
            case op_t::bv_xor:
            case op_t::bv_sub:
                return _stp.CreateNode( stp::XOR , a, b );
            case op_t::bv_add:
                return _stp.CreateNode( stp::AND, a,  b );
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
                return _stp.CreateNode( stp::OR, a, b );
            case op_t::bool_and:
            case op_t::bv_and:
                return _stp.CreateNode( stp::AND, a,  b );
            case op_t::bv_uge:
            case op_t::bv_sle:
                return _stp.CreateNode( stp::OR, a, _stp.CreateNode( stp::NOT, b ) );
            case op_t::bv_ule:
            case op_t::bv_sge:
                return _stp.CreateNode( stp::OR, b, _stp.CreateNode( stp::NOT, a ) );
            case op_t::bv_ugt:
            case op_t::bv_slt:
                return _stp.CreateNode( stp::AND, a, _stp.CreateNode( stp::NOT, b ) );
            case op_t::bv_ult:
            case op_t::bv_sgt:
                return _stp.CreateNode( stp::AND, b, _stp.CreateNode( stp::NOT, a ) );
            case op_t::eq:
                return _stp.CreateNode( stp::IFF, a, b );
            case op_t::neq:
                return _stp.CreateNode( stp::NAND, a, b );
            default:
                UNREACHABLE( "unknown boolean binary operation", op );
        }
    }
}

}
#endif
