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

using BNode = brick::smt::Node;

using namespace std::literals;
namespace smt = brick::smt;

z3::expr Z3::constant( Constant con )
{
    // Sadly, Z3 changed API: bv_val used to take __int64 or __uint64 which was
    // a define for (unsigned) long long, now it takes (u)int_64_t, which is
    // (unsinged) long on Linux. Using the wrong type causes ambiguous overload
    // in the other version, so we need this hack.
#ifdef __int64
    using ValT = __int64;
#else
    using ValT = int64_t;
#endif
    switch ( con.type() )
    {
        case BNode::Type::Int:
            return _ctx.bv_val( static_cast< ValT >( con.value ), con.bitwidth() );
        case BNode::Type::Float:
            UNREACHABLE( "Floating point arithmetic is not yet supported with Z3." );
        case BNode::Type::Bool:
            return constant( static_cast< bool >( con.value ) );
    }

    UNREACHABLE( "unknown type" );
}

z3::expr Z3::constant( bool v )
{
    return _ctx.bool_val( v );
}

z3::expr Z3::constant( smt::Bitwidth bw, uint64_t value )
{
    return _ctx.bv_val( bw, value );
}

z3::expr Z3::variable( Variable var )
{
    switch ( var.type() )
    {
        case BNode::Type::Int:
            return _ctx.bv_const( ( "var_"s + std::to_string( var.id ) ).c_str(), var.bitwidth() );
        case BNode::Type::Float:
            UNREACHABLE( "Floating point arithmetic is not yet supported with Z3." );
        case BNode::Type::Bool:
            UNREACHABLE( "Unsupported boolean variable." );
    }

    UNREACHABLE( "unknown type" );
}

z3::expr Z3::unary( Unary un, Node arg )
{
    int bw = un.bw;
    int childbw = arg.is_bv() ? arg.get_sort().bv_size() : 1;

    switch ( un.op )
    {
        case Op::Trunc:
            ASSERT_LEQ( bw, childbw );
            ASSERT( arg.is_bv() );
            if ( bw == childbw )
                return arg;
            return arg.extract( bw - 1, 0 );
        case Op::ZExt:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return arg.is_bv()
                ? z3::zext( arg, bw - childbw )
                : z3::ite( arg, _ctx.bv_val( 1, bw ), _ctx.bv_val( 0, bw ) );
        case Op::SExt:
            ASSERT_LEQ( childbw, bw );
            if ( bw == childbw )
                return arg;
            return arg.is_bv()
                ? z3::sext( arg, bw - childbw )
                : z3::ite( arg, ~_ctx.bv_val( 0, bw ), _ctx.bv_val( 0, bw ) );
        /*case Op::FPExt:
            ASSERT_LT( childbw, bw );
            // Z3_mk_fpa_to_fp_float( arg.ctx() ); // TODO
            UNREACHABLE_F( "Unsupported operation." );
        case Op::FPTrunc:
            ASSERT_LT( bw, childbw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case Op::FPToSInt:
            UNREACHABLE_F( "Unsupported operation." ); // TODO
            ASSERT_EQ( childbw, bw );
        case Op::FPToUInt:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case Op::SIntToFP:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case Op::UIntToFP:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO*/
        case Op::Not:
            ASSERT_EQ( childbw, bw );
            ASSERT_EQ( bw, 1 );
            return arg.is_bv() ? ~arg : !arg;
        case Op::Extract:
            ASSERT_LT( bw, childbw );
            ASSERT( arg.is_bv() );
            return arg.extract( un.from, un.to );
        default:
            UNREACHABLE( "unknown unary operation", un.op );
    }
}

z3::expr Z3::binary( Binary bin, Node a, Node b )
{
    if ( a.is_bv() && b.is_bv() )
    {
        switch ( bin.op )
        {
            case Op::BvAdd:  return a + b;
            case Op::BvSub:  return a - b;
            case Op::BvMul:  return a * b;
            case Op::BvSDiv: return a / b;
            case Op::BvUDiv: return z3::udiv( a, b );
            case Op::BvSRem: return z3::srem( a, b );
            case Op::BvURem: return z3::urem( a, b );
            case Op::BvShl:  return z3::shl( a, b );
            case Op::BvAShr: return z3::ashr( a, b );
            case Op::BvLShr: return z3::lshr( a, b );
            case Op::And:
            case Op::BvAnd:  return a & b;
            case Op::BvOr:   return a | b;
            case Op::BvXor:  return a ^ b;

            case Op::BvULE:  return z3::ule( a, b );
            case Op::BvULT:  return z3::ult( a, b );
            case Op::BvUGE:  return z3::uge( a, b );
            case Op::BvUGT:  return z3::ugt( a, b );
            case Op::BvSLE:  return a <= b;
            case Op::BvSLT:  return a < b;
            case Op::BvSGE:  return a >= b;
            case Op::BvSGT:  return a > b;
            case Op::Eq:   return a == b;
            case Op::NE:   return a != b;
            case Op::Concat: return z3::concat( a, b );
            default:
                UNREACHABLE( "unknown binary operation", bin.op );
        }
    }
    /*else if ( a.is_real() && b.is_real() )
    {
        switch ( bin.op )
        {
            case Op::FpAdd: return a + b;
            case Op::FpSub: return a - b;
            case Op::FpMul: return a * b;
            case Op::FpDiv: return a / b;
            case Op::FpRem:
                UNREACHABLE_F( "unsupported operation FpFrem" );
            case Op::FpFalse:
                UNREACHABLE_F( "unsupported operation FpFalse" );
            case Op::FpOEQ: return a == b;
            case Op::FpOGT: return a > b;
            case Op::FpOGE: return a >= b;
            case Op::FpOLT: return a < b;
            case Op::FpOLE: return a <= b;
            case Op::FpONE: return a != b;
            case Op::FpORD:
                UNREACHABLE_F( "unsupported operation FpORD" );
            case Op::FpUEQ: return a == b;
            case Op::FpUGT: return a > b;
            case Op::FpUGE: return a >= b;
            case Op::FpULT: return a < b;
            case Op::FpULE: return a <= b;
            case Op::FpUNE: return a != b;
            case Op::FpUNO:
                UNREACHABLE_F( "unknown binary operation FpUNO" );
            case Op::FpTrue:
                UNREACHABLE_F( "unknown binary operation FpTrue" );
            default:
                UNREACHABLE_F( "unknown binary operation %d %d", bin.op, int( Op::EQ ) );
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

        switch ( bin.op )
        {
            case Op::Xor:
            case Op::BvXor:
            case Op::BvSub:
                return a != b;
            case Op::BvAdd:
                return a && b;
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
                return a || b;
            case Op::And:
            case Op::BvAnd:
                return a && b;
            case Op::BvUGE:
            case Op::BvSLE:
                return a || !b;
            case Op::BvULE:
            case Op::BvSGE:
                return b || !a;
            case Op::BvUGT:
            case Op::BvSLT:
                return a && !b;
            case Op::BvULT:
            case Op::BvSGT:
                return b && !a;
            case Op::Eq:
                return a == b;
            case Op::NE:
                return a != b;
            default:
                UNREACHABLE( "unknown boolean binary operation", bin.op );
        }
    }
}


} // namespace divine::smt::builder
#endif
