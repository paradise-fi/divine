// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/formula.hpp>

namespace divine::vm
{

using namespace std::literals;

std::string_view FormulaMap::convert( HeapPointer ptr )
{
    auto it = ptr2Sym.find( ptr );
    if ( it != ptr2Sym.end() )
        return it->second;

    auto formula = hp2form( ptr );
    int bw = formula->type().bitwidth();

    switch ( formula->op() )
    {
        case sym::Op::Variable:
        {
            it = ptr2Sym.emplace( ptr, "var_"s + std::to_string( formula->var.id ) ).first;
            if ( inputs.insert( formula->var.id ).second )
                out << smt::declareFun( it->second, {}, type( bw ) )
                    << " ";
            return it->second;
        }
        case sym::Op::Constant:
            it = ptr2Sym.emplace(
                ptr, to_string( bw == 1 ? (formula->con.value ? smt::symbol( "true" )
                                                              : smt::symbol( "false" ))
                                        : smt::bitvec( formula->con.value, bw ) ) ).first;
            return it->second;
        case sym::Op::Assume:
            it = ptr2Sym.emplace( ptr, std::string( convert( formula->assume.value ) ) ).first;
            convert( formula->assume.constraint );
            pcparts.emplace( formula->assume.constraint );
            return it->second;
        default: ;
    }

    smt::Printer op;
    if ( sym::isUnary( formula->op() ) )
    {
        sym::Unary &unary = formula->unary;
        auto arg = smt::symbol( convert( unary.child ) );
        int childbw = hp2form( unary.child )->type().bitwidth();

        switch ( unary.op ) {
            case sym::Op::Trunc:
                ASSERT_LT( bw, childbw );
                op = smt::extract( bw - 1, 0, arg );
                if ( bw == 1 )
                    op = smt::binop< smt::Op::Eq >( op, smt::bitvec( 1, 1 ) );
                break;
            case sym::Op::ZExt:
                ASSERT_LT( childbw, bw );
                op = childbw > 1
                     ? smt::binop< smt::Op::Concat >( smt::bitvec( 0, bw - childbw ), arg )
                     : smt::ite( arg, smt::bitvec( 1, bw ), smt::bitvec( 0, bw ) );
                break;
            case sym::Op::SExt:
                ASSERT_LT( childbw, bw );
                op = childbw > 1
                     ? smt::binop< smt::Op::Concat >(
                         smt::ite( smt::binop< smt::Op::Eq >(
                                       smt::bitvec( 1, 1 ),
                                       smt::extract( childbw - 1, childbw - 1, arg ) ),
                                   smt::unop< smt::Op::BvNot >( smt::bitvec( 0, bw - childbw ) ),
                                   smt::bitvec( 0, bw - childbw ) ),
                         arg )
                     : smt::ite( arg, smt::unop< smt::Op::Not >( smt::bitvec( 0, bw ) ),
                                      smt::bitvec( 0, bw ) );
                break;
            case sym::Op::BoolNot:
                ASSERT_EQ( childbw, bw );
                ASSERT_EQ( bw, 1 );
                op = smt::unop< smt::Op::Not >( arg );
                break;
            case sym::Op::Extract:
                ASSERT_LT( bw, childbw );
                op = smt::extract( unary.from, unary.to, arg );
                break;
            default:
                UNREACHABLE_F( "unknown unary operation %d", unary.op );
        }
    }
    else if ( sym::isBinary( formula->op() ) )
    {
        sym::Binary &binary = formula->binary;
        auto a = smt::symbol( convert( binary.left ) );
        auto b = smt::symbol( convert( binary.right ) );
        auto fa = hp2form( binary.left ), fb = hp2form( binary.right );
        int abw = fa->type().bitwidth(), bbw = fb->type().bitwidth();
        ASSERT_EQ( abw, bbw );

        if ( abw > 1 ) {
            switch ( binary.op ) {
#define MAP_OP_ARITH( OP ) case sym::Op::OP:                    \
                ASSERT_EQ( bw, abw );                           \
                op = smt::binop< smt::Op::Bv ## OP >( a, b );   \
                break
                MAP_OP_ARITH( Add );
                MAP_OP_ARITH( Sub );
                MAP_OP_ARITH( Mul );
                MAP_OP_ARITH( SDiv );
                MAP_OP_ARITH( UDiv );
                MAP_OP_ARITH( SRem );
                MAP_OP_ARITH( URem );
                MAP_OP_ARITH( Shl ); // NOTE: LLVM & SMT-LIB both require args to shift to have same type
                MAP_OP_ARITH( AShr );
                MAP_OP_ARITH( LShr );
                MAP_OP_ARITH( And );
                MAP_OP_ARITH( Or );
                MAP_OP_ARITH( Xor );
#undef MAP_OP_ARITH

#define MAP_OP_CMP( OP ) case sym::Op::OP:                      \
                ASSERT_EQ( bw, 1 );                             \
                op = smt::binop< smt::Op::Bv ## OP >( a, b );   \
                break
                MAP_OP_CMP( ULE );
                MAP_OP_CMP( ULT );
                MAP_OP_CMP( UGE );
                MAP_OP_CMP( UGT );
                MAP_OP_CMP( SLE );
                MAP_OP_CMP( SLT );
                MAP_OP_CMP( SGE );
                MAP_OP_CMP( SGT );
#undef MAP_OP_CMP
                case sym::Op::EQ:
                    ASSERT_EQ( bw, 1 );
                    op = smt::binop< smt::Op::Eq >( a, b );
                    break;
                case sym::Op::NE:
                    ASSERT_EQ( bw, 1 );
                    op = smt::unop< smt::Op::Not >( smt::binop< smt::Op::Eq >( a, b ) );
                    break;
                case sym::Op::Concat:
                    ASSERT_EQ( bw, abw + bbw );
                    op = smt::binop< smt::Op::Concat >( a, b );
                    break;
                default:
                    UNREACHABLE_F( "unknown binary operation %d", binary.op );
            }
        } else {
            ASSERT_EQ( abw, 1 );
            switch ( binary.op ) {
                case sym::Op::Xor:
                case sym::Op::Add:
                case sym::Op::Sub:
                    op = smt::binop< smt::Op::Xor >( a, b );
                    break;
                case sym::Op::And:
                case sym::Op::Mul:
                    op = smt::binop< smt::Op::And >( a, b );
                    break;
                case sym::Op::UDiv:
                case sym::Op::SDiv:
                    op = a; // ?
                    break;
                case sym::Op::URem:
                case sym::Op::SRem:
                case sym::Op::Shl:
                case sym::Op::LShr:
                    op = smt::symbol( "false" );
                    break;
                case sym::Op::AShr:
                    op = a;
                    break;
                case sym::Op::Or:
                    op = smt::binop< smt::Op::Or >( a, b );
                    break;

                case sym::Op::UGE:
                case sym::Op::SLE:
                    op = smt::binop< smt::Op::Or >( a, smt::unop< smt::Op::Not >( b ) );
                    break;
                case sym::Op::ULE:
                case sym::Op::SGE:
                    op = smt::binop< smt::Op::Or >( b, smt::unop< smt::Op::Not >( a ) );
                    break;

                case sym::Op::UGT:
                case sym::Op::SLT:
                    op = smt::binop< smt::Op::And >( a, smt::unop< smt::Op::Not >( b ) );
                    break;
                case sym::Op::ULT:
                case sym::Op::SGT:
                    op = smt::binop< smt::Op::And >( b, smt::unop< smt::Op::Not >( a ) );
                    break;

                case sym::Op::EQ:
                    op = smt::binop< smt::Op::Eq >( a, b );
                    break;
                case sym::Op::NE:
                    op = smt::unop< smt::Op::Not >( smt::binop< smt::Op::Eq >( a, b ) );
                    break;
                default:
                    UNREACHABLE_F( "unknown binary operation %d", binary.op );
            }
        }
    }

    ASSERT( op );
    auto name = "val_"s + std::to_string( valcount++ ) + suff;
    out << smt::defineConst( name, type( bw ), op ) << " ";
    return ptr2Sym.emplace( ptr, name ).first->second;
}

}
