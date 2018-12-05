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

#include <divine/smt/builder-smtlib-bv.hpp>

namespace divine::smt::builder
{

using namespace std::literals;
namespace smt = brick::smt;

SMTLib2::Node SMTLib2::define( Node def )
{
    if ( !_use_defs )
        return def;
    auto name = "def_"s + std::to_string( _def_counter ++ ) + _suff;
    return _ctx.define( name, def );
}

SMTLib2::Node SMTLib2::variable( sym::Type t, int32_t id )
{
    auto name = "var_" + std::to_string( id );
    switch ( t.type() ) {
        case sym::Type::Int:
            return _ctx.variable( _ctx.bitvecT( t.bitwidth() ), name );
        case sym::Type::Float:
            ASSERT( t.bitwidth() > 1 );
            return _ctx.variable( _ctx.floatT( t.bitwidth() ), name );
    }
    UNREACHABLE_F( "unknown type" );
}

SMTLib2::Node SMTLib2::constant( sym::Type t, uint64_t val )
{
    switch ( t.type() ) {
        case sym::Type::Int:
            return _ctx.bitvec( t.bitwidth(), val );
        case sym::Type::Float:
            ASSERT( t.bitwidth() > 1 );
            return _ctx.floatv( t.bitwidth(), val );
    }
    ASSERT_EQ( t.type(), sym::Type::Int );
}

SMTLib2::Node SMTLib2::constant( bool v )
{
    return v ? _ctx.symbol( 1, Node::Type::Bool, "true" ) : _ctx.symbol( 1, Node::Type::Bool, "false" );
}

SMTLib2::Node SMTLib2::unary( sym::Unary unary, smt::Node arg )
{
    using smt::Op;
    auto bw = unary.type.bitwidth();

    switch ( unary.op )
    {
        case sym::Op::Trunc:
        {
            ASSERT_LT( bw, arg.bw );
            auto op = _ctx.extract( bw - 1, 0, arg );
            if ( bw == 1 )
                op = _ctx.binop< Op::Eq >( bw, op, _ctx.bitvec( 1, 1 ) );
            return define( op );
        }
        case sym::Op::ZExt:
            ASSERT_LT( arg.bw, bw );
            return define( arg.bw > 1
                           ? _ctx.binop< Op::Concat >( bw, _ctx.bitvec( bw - arg.bw, 0 ), arg )
                           : _ctx.ite( arg, _ctx.bitvec( bw, 1 ), _ctx.bitvec( bw, 0 ) ) );
        case sym::Op::SExt:
            ASSERT_LT( arg.bw, bw );
            if ( arg.bw == 1 )
            {
                auto ones = _ctx.unop< Op::Not >( bw, _ctx.bitvec( bw, 0 ) ),
                   zeroes = _ctx.bitvec( bw, 0 );
                return define( _ctx.ite( arg, ones, zeroes ) );
            }
            else
            {
                int extbw = bw - arg.bw;
                auto one_ext = _ctx.bitvec( extbw, brick::bitlevel::ones< uint64_t >( extbw ) ),
                       z_ext = _ctx.bitvec( extbw, 0 ),
                        sign = _ctx.binop< Op::Eq >( 1, _ctx.bitvec( 1, 1 ),
                                                          _ctx.extract( arg.bw - 1, arg.bw - 1, arg ) );
                return define( _ctx.binop< Op::Concat >( bw, _ctx.ite( sign, one_ext, z_ext ), arg ) );
            }
        case sym::Op::FPExt:
            ASSERT_LT( arg.bw, bw );
            return _ctx.cast< Op::FPExt >( bw, arg );
        case sym::Op::FPTrunc:
            ASSERT_LT( bw, arg.bw );
            return _ctx.cast< Op::FPTrunc >( bw, arg );
        case sym::Op::FPToSInt:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::FPToSInt >( bw, arg );
        case sym::Op::FPToUInt:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::FPToUInt >( bw, arg );
        case sym::Op::SIntToFP:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::SIntToFP >( bw, arg );
        case sym::Op::UIntToFP:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::UIntToFP >( bw, arg );
        case sym::Op::BoolNot:
            ASSERT_EQ( arg.bw, bw );
            ASSERT_EQ( bw, 1 );
            return define( _ctx.unop< Op::Not >( 1, arg ) );
        case sym::Op::Extract:
            ASSERT_LT( bw, arg.bw );
            return define( _ctx.extract( unary.from, unary.to, arg ) );
        default:
            UNREACHABLE_F( "unknown unary operation %d", unary.op );
    }
}

SMTLib2::Node SMTLib2::binary( sym::Binary bin, smt::Node a, smt::Node b )
{

    using smt::Op;
    int bw = bin.type.bitwidth();
    if ( a.is_bv() && b.is_bv() )
    {
        ASSERT_EQ( a.bw, b.bw );
        switch ( bin.op )
        {
#define MAP_OP_ARITH( OP ) case sym::Op::OP:                            \
            ASSERT_EQ( bw, a.bw );                                      \
            return define( _ctx.binop< Op::Bv ## OP >( bw, a, b ) );
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

#define MAP_OP_CMP( OP ) case sym::Op::OP:                              \
            ASSERT_EQ( bw, 1 );                                         \
            return define( _ctx.binop< Op::Bv ## OP >( bw, a, b ) );
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
                return define( _ctx.binop< Op::Eq >( bw, a, b ) );
            case sym::Op::NE:
                ASSERT_EQ( bw, 1 );
                return define( _ctx.unop< Op::Not >( bw, _ctx.binop< Op::Eq >( bw, a, b ) ) );
            case sym::Op::Concat:
                ASSERT_EQ( bw, a.bw + b.bw );
                return define( _ctx.binop< Op::Concat >( bw, a, b ) );
            default:
                UNREACHABLE_F( "unknown binary operation %d", bin.op );
        }
    }
    else if ( a.is_float() && b.is_float() )
    {
        smt::Node node = a;
        switch ( bin.op )
        {
#define MAP_OP_FLOAT_ARITH( OP ) case sym::Op::OP:                      \
            ASSERT_EQ( bw, a.bw );                                      \
            return define( _ctx.fpbinop< Op:: OP >( bw, a, b ) );
            MAP_OP_FLOAT_ARITH( FpAdd );
            MAP_OP_FLOAT_ARITH( FpSub );
            MAP_OP_FLOAT_ARITH( FpMul );
            MAP_OP_FLOAT_ARITH( FpDiv );
            MAP_OP_FLOAT_ARITH( FpRem );
#undef MAP_OP_FLOAT_ARITH

#define MAP_OP_FCMP( OP ) case sym::Op::OP:                             \
            ASSERT_EQ( bw, 1 );                                         \
            return define( _ctx.fpbinop< Op:: OP >( bw, a, b ) );
            MAP_OP_FCMP( FpFalse );
            MAP_OP_FCMP( FpOEQ );
            MAP_OP_FCMP( FpOGT );
            MAP_OP_FCMP( FpOGE );
            MAP_OP_FCMP( FpOLT );
            MAP_OP_FCMP( FpOLE );
            MAP_OP_FCMP( FpONE );
            MAP_OP_FCMP( FpORD );
            MAP_OP_FCMP( FpUEQ );
            MAP_OP_FCMP( FpUGT );
            MAP_OP_FCMP( FpUGE );
            MAP_OP_FCMP( FpULT );
            MAP_OP_FCMP( FpULE );
            MAP_OP_FCMP( FpUNE );
            MAP_OP_FCMP( FpUNO );
            MAP_OP_FCMP( FpTrue );
#undef MAP_OP_FCMP
            default:
                UNREACHABLE_F( "unknown binary operation %d", bin.op );
        }
    }
    else
    {
        ASSERT( !a.is_float() );
        ASSERT( !b.is_float() );

        if ( a.is_bv() )
        {
            ASSERT_EQ( a.bw, 1 );
            a = define( _ctx.binop< Op::Eq >( 1, a, _ctx.bitvec( 1, 1 ) ) );
        }

        if ( b.is_bv() )
        {
            ASSERT_EQ( b.bw, 1 );
            b = define( _ctx.binop< Op::Eq >( 1, b, _ctx.bitvec( 1, 1 ) ) );
        }

        switch ( bin.op )
        {
            case sym::Op::Xor:
            case sym::Op::Sub:
                return define( _ctx.binop< Op::Xor >( 1, a, b ) );
            case sym::Op::Add:
                return define( _ctx.binop< Op::And >( 1, a, b ) );
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
                return define( _ctx.binop< Op::Or >( 1, a, b ) );
            case sym::Op::And:
                return define( _ctx.binop< Op::And >( 1, a, b ) );
            case sym::Op::UGE:
            case sym::Op::SLE:
                return define( _ctx.binop< Op::Or >( 1, a, _ctx.unop< Op::Not >( 1, b ) ) );
            case sym::Op::ULE:
            case sym::Op::SGE:
                return define( _ctx.binop< Op::Or >( 1, b, _ctx.unop< Op::Not >( 1, a ) ) );
            case sym::Op::UGT:
            case sym::Op::SLT:
                return define( _ctx.binop< Op::And >( 1, a, _ctx.unop< Op::Not >( 1, b ) ) );
            case sym::Op::ULT:
            case sym::Op::SGT:
                return define( _ctx.binop< Op::And >( 1, b, _ctx.unop< Op::Not >( 1, a ) ) );
            case sym::Op::EQ:
                return define( _ctx.binop< Op::Eq >( 1, a, b ) );
            case sym::Op::NE:
                return define( _ctx.unop< Op::Not >( 1, _ctx.binop< Op::And >( 1, a, b ) ) );
            default:
                UNREACHABLE_F( "unknown boolean binary operation %d", bin.op );
        }
    }
}

} // namespace divine::smtl::builder
