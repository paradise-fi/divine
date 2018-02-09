// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
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

#include <divine/smt/builder.hpp>

namespace divine::smt::builder
{

using namespace std::literals;
namespace smt = brick::smt;

SMTLib2::Node SMTLib2::define( Node def )
{
    auto name = "def_"s + std::to_string( _def_counter ++ ) + _suff;
    return _ctx.define( name, def );
}

SMTLib2::Node SMTLib2::variable( sym::Type t, int32_t id )
{
    auto name = "var_" + std::to_string( id );
    return t.bitwidth() == 1 ? _ctx.variable( _ctx.boolT(), name )
                             : _ctx.variable( t.bitwidth(), name );
}

SMTLib2::Node SMTLib2::constant( sym::Type t, uint64_t val )
{
    return t.bitwidth() == 1 ? constant( bool( val ) ) : _ctx.bitvec( t.bitwidth(), val );
}

SMTLib2::Node SMTLib2::constant( bool v )
{
    return v ? _ctx.symbol( 1, "true" ) : _ctx.symbol( 1, "false" );
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
    ASSERT_EQ( a.bw, b.bw );

    if ( a.bw > 1 )
    {
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
    else
    {
        ASSERT_EQ( a.bw, 1 );
        switch ( bin.op )
        {
            case sym::Op::Xor:
            case sym::Op::Add:
            case sym::Op::Sub:
                return define( _ctx.binop< Op::Xor >( 1, a, b ) );
            case sym::Op::And:
            case sym::Op::Mul:
                return define( _ctx.binop< Op::And >( 1, a, b ) );
            case sym::Op::UDiv:
            case sym::Op::SDiv:
                return a; // ?
            case sym::Op::URem:
            case sym::Op::SRem:
            case sym::Op::Shl:
            case sym::Op::LShr:
                return _ctx.symbol( 1, "false" );
            case sym::Op::AShr:
                return a;
            case sym::Op::Or:
                return define( _ctx.binop< Op::Or >( 1, a, b ) );

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
                return define( _ctx.unop< Op::Not >( 1, _ctx.binop< Op::Eq >( 1, a, b ) ) );
            default:
                UNREACHABLE_F( "unknown binary operation %d", bin.op );
        }
    }
}

#if OPT_Z3

z3::expr Z3::constant( sym::Type t, uint64_t v )
{
    return _ctx.bv_val( static_cast< __int64 >( v ), t.bitwidth() );
}

z3::expr Z3::constant( bool v )
{
    return _ctx.bool_val( v );
}

z3::expr Z3::variable( sym::Type t, int32_t id )
{
    return _ctx.bv_const( ( "var_"s + std::to_string( id ) ).c_str(), t.bitwidth() );
}

z3::expr Z3::unary( sym::Unary un, Node arg )
{
    int bw = un.type.bitwidth();
    int childbw = arg.is_bv() ? arg.get_sort().bv_size() : 1;

    switch ( un.op )
    {
        case sym::Op::Trunc:
            ASSERT_LT( bw, childbw );
            ASSERT( arg.is_bv() );
            return arg.extract( bw - 1, 0 );
        case sym::Op::ZExt:
            ASSERT_LT( childbw, bw );
            return arg.is_bv()
                ? z3::zext( arg, bw - childbw )
                : z3::ite( arg, _ctx.bv_val( 1, bw ), _ctx.bv_val( 0, bw ) );
        case sym::Op::SExt:
            ASSERT_LT( childbw, bw );
            return arg.is_bv()
                ? z3::sext( arg, bw - childbw )
                : z3::ite( arg, ~_ctx.bv_val( 0, bw ), _ctx.bv_val( 0, bw ) );
        case sym::Op::BoolNot:
            ASSERT_EQ( childbw, bw );
            ASSERT_EQ( bw, 1 );
            return arg.is_bv() ? ~arg : !arg;
        case sym::Op::Extract:
            ASSERT_LT( bw, childbw );
            ASSERT( arg.is_bv() );
            return arg.extract( un.from, un.to );
        default:
            UNREACHABLE_F( "unknown unary operation %d", un.op );
    }
}

z3::expr Z3::binary( sym::Binary bin, Node a, Node b )
{
    ASSERT( sym::isBinary( bin.op ) );

    if ( a.is_bv() && b.is_bv() )
    {
        switch ( bin.op )
        {
            case sym::Op::Add:  return a + b;
            case sym::Op::Sub:  return a - b;
            case sym::Op::Mul:  return a * b;
            case sym::Op::SDiv: return a / b;
            case sym::Op::UDiv: return z3::udiv( a, b );
            case sym::Op::SRem: return z3::srem( a, b );
            case sym::Op::URem: return z3::urem( a, b );
            case sym::Op::Shl:  return z3::shl( a, b );
            case sym::Op::AShr: return z3::ashr( a, b );
            case sym::Op::LShr: return z3::lshr( a, b );
            case sym::Op::And:  return a & b;
            case sym::Op::Or:   return a | b;
            case sym::Op::Xor:  return a ^ b;

            case sym::Op::ULE:  return z3::ule( a, b );
            case sym::Op::ULT:  return z3::ult( a, b );
            case sym::Op::UGE:  return z3::uge( a, b );
            case sym::Op::UGT:  return z3::ugt( a, b );
            case sym::Op::SLE:  return a <= b;
            case sym::Op::SLT:  return a < b;
            case sym::Op::SGE:  return a >= b;
            case sym::Op::SGT:  return a > b;
            case sym::Op::EQ:   return a == b;
            case sym::Op::NE:   return a != b;
            case sym::Op::Concat: return z3::concat( a, b );
            default:
                UNREACHABLE_F( "unknown binary operation %d", bin.op );
        }
    }
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

        switch ( bin.op )
        {
            case sym::Op::And:  return a && b;
            case sym::Op::Or:   return a || b;
            case sym::Op::EQ:   return a == b;
            default:
                UNREACHABLE_F( "unknown binary operation %d", bin.op );
        }
    }
}
#endif

}
