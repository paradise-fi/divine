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

#if 0
#if OPT_STP
namespace divine::smt::builder
{

using namespace std::literals;
namespace smt = brick::smt;

stp::ASTNode STP::constant( sym::Type t, uint64_t v )
{
    ASSERT_NEQ( t.type(), sym::Type::Float );
    return constant( t.bitwidth(), v );
}

stp::ASTNode STP::constant( int w, uint64_t v )
{
    return _stp.CreateBVConst( w, v );
}

stp::ASTNode STP::constant( int v )
{
    return constant( 32, v );
}

stp::ASTNode STP::constant( bool v )
{
    return v ? _stp.ASTTrue : _stp.ASTFalse;
}

stp::ASTNode STP::variable( sym::Type t, int32_t id )
{
    ASSERT_NEQ( t.type(), sym::Type::Float );
    return _stp.CreateSymbol( ( "var_"s + std::to_string( id ) ).c_str(), 0, t.bitwidth() );
}

static bool is_bv( stp::ASTNode n ) { return n.GetType() == stp::BITVECTOR_TYPE; }

stp::ASTNode STP::unary( sym::Unary un, Node arg )
{
    int bw = un.type.bitwidth();
    int childbw = is_bv( arg ) ? arg.GetValueWidth() : 1;

    switch ( un.op )
    {
        case sym::Op::Trunc:
            ASSERT_LT( bw, childbw );
            ASSERT( is_bv( arg ) );
            return _stp.CreateTerm( stp::BVEXTRACT, bw, arg, constant( bw - 1 ), constant( 0 ) );
        case sym::Op::ZExt:
            ASSERT_LT( childbw, bw );
            return is_bv( arg )
                ? _stp.CreateTerm( stp::BVCONCAT, bw, constant( bw - childbw, 0 ), arg )
                : _stp.CreateTerm( stp::ITE, bw, arg, constant( bw, 1 ), constant( bw, 0 ) );
        case sym::Op::SExt:
            ASSERT_LT( childbw, bw );
            return is_bv( arg )
                ? _stp.CreateTerm( stp::BVSX, bw, arg, constant( bw ) )
                : _stp.CreateTerm( stp::ITE, bw, arg,
                                   constant( bw, brick::bitlevel::ones< uint64_t >( bw )  ),
                                   constant( bw, 0 ) );
        case sym::Op::BoolNot:
            ASSERT_EQ( childbw, bw );
            ASSERT_EQ( bw, 1 );
            return is_bv( arg ) ? _stp.CreateTerm( stp::BVNOT, childbw, arg )
                                : _stp.CreateNode( stp::NOT, arg );
        case sym::Op::Extract:
            ASSERT_LT( bw, childbw );
            ASSERT( is_bv( arg ) );
            return _stp.CreateTerm( stp::BVEXTRACT, bw, arg, constant( un.from ), constant( un.to ) );
        default:
            UNREACHABLE_F( "unknown unary operation %d", un.op );
    }
}

stp::ASTNode STP::binary( sym::Binary bin, Node a, Node b )
{
    ASSERT( sym::isBinary( bin.op ) );
    int bw = bin.type.bitwidth();

    if ( is_bv( a ) && is_bv( b ) )
    {
        switch ( bin.op )
        {
            case sym::Op::Add:    return _stp.CreateTerm( stp::BVPLUS,       bw, a, b );
            case sym::Op::Sub:    return _stp.CreateTerm( stp::BVSUB,        bw, a, b );
            case sym::Op::Mul:    return _stp.CreateTerm( stp::BVMULT,       bw, a, b );
            case sym::Op::SDiv:   return _stp.CreateTerm( stp::SBVDIV,       bw, a, b );
            case sym::Op::UDiv:   return _stp.CreateTerm( stp::BVDIV,        bw, a, b );
            case sym::Op::SRem:   return _stp.CreateTerm( stp::SBVREM,       bw, a, b );
            case sym::Op::URem:   return _stp.CreateTerm( stp::BVMOD,        bw, a, b );
            case sym::Op::Shl:    return _stp.CreateTerm( stp::BVLEFTSHIFT,  bw, a, b );
            case sym::Op::AShr:   return _stp.CreateTerm( stp::BVSRSHIFT,    bw, a, b );
            case sym::Op::LShr:   return _stp.CreateTerm( stp::BVRIGHTSHIFT, bw, a, b );
            case sym::Op::And:    return _stp.CreateTerm( stp::BVAND,        bw, a, b );
            case sym::Op::Or:     return _stp.CreateTerm( stp::BVOR,         bw, a, b );
            case sym::Op::Xor:    return _stp.CreateTerm( stp::BVXOR,        bw, a, b );
            case sym::Op::Concat: return _stp.CreateTerm( stp::BVCONCAT, bw, a, b );

            case sym::Op::ULE:  return _stp.CreateNode( stp::BVLE,  a, b );
            case sym::Op::ULT:  return _stp.CreateNode( stp::BVLT,  a, b );
            case sym::Op::UGE:  return _stp.CreateNode( stp::BVGE,  a, b );
            case sym::Op::UGT:  return _stp.CreateNode( stp::BVGT,  a, b );
            case sym::Op::SLE:  return _stp.CreateNode( stp::BVSLE, a, b );
            case sym::Op::SLT:  return _stp.CreateNode( stp::BVSLT, a, b );
            case sym::Op::SGE:  return _stp.CreateNode( stp::BVSGE, a, b );
            case sym::Op::SGT:  return _stp.CreateNode( stp::BVSGT, a, b );
            case sym::Op::EQ:   return _stp.CreateNode( stp::EQ,    a, b );
            case sym::Op::NE:   return _stp.CreateNode( stp::NOT, _stp.CreateNode( stp::EQ, a, b ) );

            default:
                UNREACHABLE_F( "unknown binary operation %d", bin.op );
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
            case sym::Op::Xor:
            case sym::Op::Sub:
                return _stp.CreateNode( stp::XOR , a, b );
            case sym::Op::Add:
                return _stp.CreateNode( stp::AND, a,  b );
            case sym::Op::UDiv:
            case sym::Op::SDiv:
                return a;
            case sym::Op::URem:
            case sym::Op::SRem:
            case sym::Op::Shl:
            case sym::Op::LShr:
                return constant( false );
            case sym::Op::AShr:
                return a;
            case sym::Op::Or:
                return _stp.CreateNode( stp::OR, a, b );
            case sym::Op::And:
                return _stp.CreateNode( stp::AND, a,  b );
            case sym::Op::UGE:
            case sym::Op::SLE:
                return _stp.CreateNode( stp::OR, a, _stp.CreateNode( stp::NOT, b ) );
            case sym::Op::ULE:
            case sym::Op::SGE:
                return _stp.CreateNode( stp::OR, b, _stp.CreateNode( stp::NOT, a ) );
            case sym::Op::UGT:
            case sym::Op::SLT:
                return _stp.CreateNode( stp::AND, a, _stp.CreateNode( stp::NOT, b ) );
            case sym::Op::ULT:
            case sym::Op::SGT:
                return _stp.CreateNode( stp::AND, b, _stp.CreateNode( stp::NOT, a ) );
            case sym::Op::EQ:
                return _stp.CreateNode( stp::IFF, a, b );
            case sym::Op::NE:
                return _stp.CreateNode( stp::NAND, a, b );
            default:
                UNREACHABLE_F( "unknown boolean binary operation %d", bin.op );
        }
    }
}

}
#endif
#endif
