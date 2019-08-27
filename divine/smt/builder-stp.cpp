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
using BNode = brick::smt::Node;

using namespace std::literals;
namespace smt = brick::smt;

stp::ASTNode STP::constant( Constant con )
{
    ASSERT( con.type() != BNode::Type::Float );
    return constant( con.bitwidth, con.value );
}

stp::ASTNode STP::constant( smt::Bitwidth bw, uint64_t value )
{
    return _stp.CreateBVConst( bw, value );
}

stp::ASTNode STP::constant( int value )
{
    return constant( 32, value );
}

stp::ASTNode STP::constant( bool v )
{
    return v ? _stp.ASTTrue : _stp.ASTFalse;
}

stp::ASTNode STP::variable( Variable var )
{
    ASSERT( var.type() != BNode::Type::Float );
    return _stp.CreateSymbol( ( "var_"s + std::to_string( var.id ) ).c_str(), 0, var.bitwidth() );
}

static bool is_bv( stp::ASTNode n ) { return n.GetType() == stp::BITVECTOR_TYPE; }

stp::ASTNode STP::unary( Unary un, Node arg )
{
    int bw = un.bw;
    int childbw = is_bv( arg ) ? arg.GetValueWidth() : 1;

    switch ( un.op )
    {
        case Op::Trunc:
            ASSERT_LEQ( bw, childbw );
            ASSERT( is_bv( arg ) );
            if ( bw == childbw )
                return arg;
            return _stp.CreateTerm( stp::BVEXTRACT, bw, arg, constant( bw - 1 ), constant( 0 ) );
        case Op::ZExt:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return is_bv( arg )
                ? _stp.CreateTerm( stp::BVCONCAT, bw, constant( bw - childbw, 0 ), arg )
                : _stp.CreateTerm( stp::ITE, bw, arg, constant( bw, 1 ), constant( bw, 0 ) );
        case Op::SExt:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return is_bv( arg )
                ? _stp.CreateTerm( stp::BVSX, bw, arg, constant( bw ) )
                : _stp.CreateTerm( stp::ITE, bw, arg,
                                   constant( bw, brick::bitlevel::ones< uint64_t >( bw )  ),
                                   constant( bw, 0 ) );
        case Op::Not:
            ASSERT_EQ( childbw, bw );
            ASSERT_EQ( bw, 1 );
            return is_bv( arg ) ? _stp.CreateTerm( stp::BVNOT, childbw, arg )
                                : _stp.CreateNode( stp::NOT, arg );
        /*case Op::Extract:
            ASSERT_LT( bw, childbw );
            ASSERT( is_bv( arg ) );
            return _stp.CreateTerm( stp::BVEXTRACT, bw, arg, constant( un.from ), constant( un.to ) );*/
        default:
            UNREACHABLE( "unknown unary operation", un.op );
    }
}

stp::ASTNode STP::binary( Binary bin, Node a, Node b )
{
    auto bw = bin.bw;

    if ( is_bv( a ) && is_bv( b ) )
    {
        switch ( bin.op )
        {
            case Op::BvAdd:    return _stp.CreateTerm( stp::BVPLUS,       bw, a, b );
            case Op::BvSub:    return _stp.CreateTerm( stp::BVSUB,        bw, a, b );
            case Op::BvMul:    return _stp.CreateTerm( stp::BVMULT,       bw, a, b );
            case Op::BvSDiv:   return _stp.CreateTerm( stp::SBVDIV,       bw, a, b );
            case Op::BvUDiv:   return _stp.CreateTerm( stp::BVDIV,        bw, a, b );
            case Op::BvSRem:   return _stp.CreateTerm( stp::SBVREM,       bw, a, b );
            case Op::BvURem:   return _stp.CreateTerm( stp::BVMOD,        bw, a, b );
            case Op::BvShl:    return _stp.CreateTerm( stp::BVLEFTSHIFT,  bw, a, b );
            case Op::BvAShr:   return _stp.CreateTerm( stp::BVSRSHIFT,    bw, a, b );
            case Op::BvLShr:   return _stp.CreateTerm( stp::BVRIGHTSHIFT, bw, a, b );
            case Op::And:
            case Op::BvAnd:    return _stp.CreateTerm( stp::BVAND,        bw, a, b );
            case Op::BvOr:     return _stp.CreateTerm( stp::BVOR,         bw, a, b );
            case Op::BvXor:    return _stp.CreateTerm( stp::BVXOR,        bw, a, b );
            case Op::Concat:   return _stp.CreateTerm( stp::BVCONCAT,     bw, a, b );

            case Op::BvULE:  return _stp.CreateNode( stp::BVLE,  a, b );
            case Op::BvULT:  return _stp.CreateNode( stp::BVLT,  a, b );
            case Op::BvUGE:  return _stp.CreateNode( stp::BVGE,  a, b );
            case Op::BvUGT:  return _stp.CreateNode( stp::BVGT,  a, b );
            case Op::BvSLE:  return _stp.CreateNode( stp::BVSLE, a, b );
            case Op::BvSLT:  return _stp.CreateNode( stp::BVSLT, a, b );
            case Op::BvSGE:  return _stp.CreateNode( stp::BVSGE, a, b );
            case Op::BvSGT:  return _stp.CreateNode( stp::BVSGT, a, b );
            case Op::Eq:     return _stp.CreateNode( stp::EQ,    a, b );
            case Op::NE:     return _stp.CreateNode( stp::NOT, _stp.CreateNode( stp::EQ, a, b ) );

            default:
                UNREACHABLE( "unknown binary operation", bin.op );
        }
    }
    else
    {
        if ( is_bv( a ) )
        {
            ASSERT_EQ( a.GetValueWidth(), 1 );
            a = _stp.CreateNode( stp::EQ, a, constant( 1, 1 ) );
        }

        if ( is_bv( b ) )
        {
            ASSERT_EQ( b.GetValueWidth(), 1 );
            b = _stp.CreateNode( stp::EQ, b, constant( 1, 1 ) );
        }

        switch ( bin.op )
        {
            case Op::Xor:
            case Op::BvXor:
            case Op::BvSub:
                return _stp.CreateNode( stp::XOR , a, b );
            case Op::BvAdd:
                return _stp.CreateNode( stp::AND, a,  b );
            case Op::BvUDiv:
            case Op::BvSDiv:
                return a;
            case Op::BvURem:
            case Op::BvSRem:
            case Op::BvShl:
            case Op::BvLShr:
                return constant( false );
            case Op::BvAShr:
                return a;
            case Op::Or:
            case Op::BvOr:
                return _stp.CreateNode( stp::OR, a, b );
            case Op::And:
            case Op::BvAnd:
                return _stp.CreateNode( stp::AND, a,  b );
            case Op::BvUGE:
            case Op::BvSLE:
                return _stp.CreateNode( stp::OR, a, _stp.CreateNode( stp::NOT, b ) );
            case Op::BvULE:
            case Op::BvSGE:
                return _stp.CreateNode( stp::OR, b, _stp.CreateNode( stp::NOT, a ) );
            case Op::BvUGT:
            case Op::BvSLT:
                return _stp.CreateNode( stp::AND, a, _stp.CreateNode( stp::NOT, b ) );
            case Op::BvULT:
            case Op::BvSGT:
                return _stp.CreateNode( stp::AND, b, _stp.CreateNode( stp::NOT, a ) );
            case Op::Eq:
                return _stp.CreateNode( stp::IFF, a, b );
            case Op::NE:
                return _stp.CreateNode( stp::NAND, a, b );
            default:
                UNREACHABLE( "unknown boolean binary operation", bin.op );
        }
    }
}

}
#endif
