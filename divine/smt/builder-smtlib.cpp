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
namespace smt = brick::smt;

SMTLib2::Node SMTLib2::define( Node def )
{
    if ( !_use_defs )
        return def;
    auto name = "def_"s + std::to_string( _def_counter ++ ) + _suff;
    return _ctx.define( name, def );
}

SMTLib2::Node SMTLib2::variable( Variable var )
{
    auto name = "var_" + std::to_string( var.id );
    switch ( var.type() ) {
        case Node::Type::Bool:
            ASSERT_EQ( var.bitwidth(), 1 );
            return _ctx.variable( _ctx.boolT(), name );
        case Node::Type::Int:
            return _ctx.variable( _ctx.bitvecT( var.bitwidth() ), name );
        case Node::Type::Float:
            ASSERT( var.bitwidth() > 1 );
            return _ctx.variable( _ctx.floatT( var.bitwidth() ), name );
    }
}

SMTLib2::Node SMTLib2::constant( Constant con )
{
   switch ( con.type() )
   {
        case Node::Type::Bool:
            ASSERT_EQ( con.bitwidth(), 1 );
            return constant( static_cast< bool >( con.value ) );
        case Node::Type::Int:
            return _ctx.bitvec( con.bitwidth(), con.value );
        case Node::Type::Float:
            ASSERT( con.bitwidth() > 1 );
            return _ctx.floatv( con.bitwidth(), con.value );
    }
    UNREACHABLE( "unknown constant type", con.type() );
}

SMTLib2::Node SMTLib2::constant( bool v )
{
    return v ? _ctx.symbol( 1, Node::Type::Bool, "true" ) : _ctx.symbol( 1, Node::Type::Bool, "false" );
}

SMTLib2::Node SMTLib2::constant( smt::Bitwidth bw, uint64_t value )
{
    return _ctx.bitvec( bw, value );
}

SMTLib2::Node SMTLib2::unary( Unary unary, Node arg )
{
    auto bw = unary.bw;

    switch ( unary.op )
    {
        case Op::Trunc:
        {
            ASSERT_LEQ( bw, arg.bw );
            if ( arg.bw == bw )
                return arg;
            auto op = _ctx.extract( bw - 1, 0, arg );
            if ( bw == 1 )
                op = _ctx.binop< Op::Eq >( bw, op, _ctx.bitvec( 1, 1 ) );
            return define( op );
        }
        case Op::ZExt:
            ASSERT_LEQ( arg.bw, bw );
            if ( arg.bw == bw )
                return arg;
            return define( arg.bw > 1
                           ? _ctx.binop< Op::Concat >( bw, _ctx.bitvec( bw - arg.bw, 0 ), arg )
                           : _ctx.ite( arg, _ctx.bitvec( bw, 1 ), _ctx.bitvec( bw, 0 ) ) );
        case Op::SExt:
            ASSERT_LEQ( arg.bw, bw );
            if ( arg.bw == bw )
                return arg;
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
        case Op::FPExt:
            ASSERT_LT( arg.bw, bw );
            return _ctx.cast< Op::FPExt >( bw, arg );
        case Op::FPTrunc:
            ASSERT_LT( bw, arg.bw );
            return _ctx.cast< Op::FPTrunc >( bw, arg );
        case Op::FPToSInt:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::FPToSInt >( bw, arg );
        case Op::FPToUInt:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::FPToUInt >( bw, arg );
        case Op::SIntToFP:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::SIntToFP >( bw, arg );
        case Op::UIntToFP:
            ASSERT_EQ( arg.bw, bw );
            return _ctx.cast< Op::UIntToFP >( bw, arg );
        case Op::Not:
            ASSERT_EQ( arg.bw, bw );
            ASSERT_EQ( bw, 1 );
            return define( arg.is_bool() ? _ctx.unop< Op::Not >( 1, arg )
                                         : _ctx.unop< Op::BvNot >( 1, arg ) );
        //case Op::Extract:
        //   ASSERT_LT( bw, arg.bw );
        //    return define( _ctx.extract( unary.from, unary.to, arg ) );
        default:
            UNREACHABLE( "unknown unary operation", unary.op );
    }
}

SMTLib2::Node SMTLib2::binary( Binary bin, Node a, Node b )
{
    auto bw = bin.bw;
    if ( a.is_bv() && b.is_bv() )
    {
        ASSERT_EQ( a.bw, b.bw );
        switch ( bin.op )
        {
#define MAP_OP_ARITH( OP ) case Op::Bv ## OP:                          \
            ASSERT_EQ( bw, a.bw );                                 \
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

#define MAP_OP_CMP( OP ) case Op::Bv ## OP:                        \
            ASSERT_EQ( bw, 1 );                                    \
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
            case Op::Eq:
                ASSERT_EQ( bw, 1 );
                return define( _ctx.binop< Op::Eq >( bw, a, b ) );
            case Op::NE:
                ASSERT_EQ( bw, 1 );
                return define( _ctx.unop< Op::Not >( bw, _ctx.binop< Op::Eq >( bw, a, b ) ) );
            case Op::Concat:
                ASSERT_EQ( bw, a.bw + b.bw );
                return define( _ctx.binop< Op::Concat >( bw, a, b ) );
            default:
                UNREACHABLE( "unknown binary operation", bin.op );
        }
    }
    else if ( a.is_float() && b.is_float() )
    {
        smt::Node node = a;
        switch ( bin.op )
        {
#define MAP_OP_FLOAT_ARITH( OP ) case Op::OP:                   \
            ASSERT_EQ( bw, a.bw );                              \
            return define( _ctx.fpbinop< Op:: OP >( bw, a, b ) );
            MAP_OP_FLOAT_ARITH( FpAdd );
            MAP_OP_FLOAT_ARITH( FpSub );
            MAP_OP_FLOAT_ARITH( FpMul );
            MAP_OP_FLOAT_ARITH( FpDiv );
            MAP_OP_FLOAT_ARITH( FpRem );
#undef MAP_OP_FLOAT_ARITH

#define MAP_OP_FCMP( OP ) case Op::OP:                          \
            ASSERT_EQ( bw, 1 );                                 \
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
                UNREACHABLE( "unknown binary operation", bin.op );
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
            case Op::Xor:
            case Op::BvSub:
            case Op::BvXor:
                return define( _ctx.binop< Op::Xor >( 1, a, b ) );
            case Op::BvAdd:
                return define( _ctx.binop< Op::And >( 1, a, b ) );
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
                return define( _ctx.binop< Op::Or >( 1, a, b ) );
            case Op::And:
            case Op::BvAnd:
                return define( _ctx.binop< Op::And >( 1, a, b ) );
            case Op::BvUGE:
            case Op::BvSLE:
                return define( _ctx.binop< Op::Or >( 1, a, _ctx.unop< Op::Not >( 1, b ) ) );
            case Op::BvULE:
            case Op::BvSGE:
                return define( _ctx.binop< Op::Or >( 1, b, _ctx.unop< Op::Not >( 1, a ) ) );
            case Op::BvUGT:
            case Op::BvSLT:
                return define( _ctx.binop< Op::And >( 1, a, _ctx.unop< Op::Not >( 1, b ) ) );
            case Op::BvULT:
            case Op::BvSGT:
                return define( _ctx.binop< Op::And >( 1, b, _ctx.unop< Op::Not >( 1, a ) ) );
            case Op::Eq:
                return define( _ctx.binop< Op::Eq >( 1, a, b ) );
            case Op::NE:
                return define( _ctx.unop< Op::Not >( 1, _ctx.binop< Op::And >( 1, a, b ) ) );
            default:
                UNREACHABLE( "unknown boolean binary operation", bin.op );
        }
    }
    UNREACHABLE( "unexpected operands", a, b, "to", bin.op );
}

} // namespace divine::smt::builder
