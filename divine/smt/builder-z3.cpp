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
#if 0
#if OPT_Z3
namespace divine::smt::builder
{

using namespace std::literals;
namespace smt = brick::smt;

z3::expr Z3::constant( sym::Type t, uint64_t v )
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
    switch ( t.type() ) {
        case sym::Type::Int:
            return _ctx.bv_val( static_cast< ValT >( v ), t.bitwidth() );
        case sym::Type::Float:
            UNREACHABLE_F( "Floatigpoint is not yet supported with z3 solver." );
    }

    UNREACHABLE_F( "unknown type" );
}

z3::expr Z3::constant( bool v )
{
    return _ctx.bool_val( v );
}

z3::expr Z3::variable( sym::Type t, int32_t id )
{
    switch ( t.type() ) {
        case sym::Type::Int:
            return _ctx.bv_const( ( "var_"s + std::to_string( id ) ).c_str(), t.bitwidth() );
        case sym::Type::Float:
            UNREACHABLE_F( "Floatigpoint is not yet supported with z3 solver." );
    }

    UNREACHABLE_F( "unknown type" );
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
        /*case sym::Op::FPExt:
            ASSERT_LT( childbw, bw );
            // Z3_mk_fpa_to_fp_float( arg.ctx() ); // TODO
            UNREACHABLE_F( "Unsupported operation." );
        case sym::Op::FPTrunc:
            ASSERT_LT( bw, childbw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case sym::Op::FPToSInt:
            UNREACHABLE_F( "Unsupported operation." ); // TODO
            ASSERT_EQ( childbw, bw );
        case sym::Op::FPToUInt:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case sym::Op::SIntToFP:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO
        case sym::Op::UIntToFP:
            ASSERT_EQ( childbw, bw );
            UNREACHABLE_F( "Unsupported operation." ); // TODO*/
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
    /*else if ( a.is_real() && b.is_real() )
    {
        switch ( bin.op )
        {
            case sym::Op::FpAdd: return a + b;
            case sym::Op::FpSub: return a - b;
            case sym::Op::FpMul: return a * b;
            case sym::Op::FpDiv: return a / b;
            case sym::Op::FpRem:
                UNREACHABLE_F( "unsupported operation FpFrem" );
            case sym::Op::FpFalse:
                UNREACHABLE_F( "unsupported operation FpFalse" );
            case sym::Op::FpOEQ: return a == b;
            case sym::Op::FpOGT: return a > b;
            case sym::Op::FpOGE: return a >= b;
            case sym::Op::FpOLT: return a < b;
            case sym::Op::FpOLE: return a <= b;
            case sym::Op::FpONE: return a != b;
            case sym::Op::FpORD:
                UNREACHABLE_F( "unsupported operation FpORD" );
            case sym::Op::FpUEQ: return a == b;
            case sym::Op::FpUGT: return a > b;
            case sym::Op::FpUGE: return a >= b;
            case sym::Op::FpULT: return a < b;
            case sym::Op::FpULE: return a <= b;
            case sym::Op::FpUNE: return a != b;
            case sym::Op::FpUNO:
                UNREACHABLE_F( "unknown binary operation FpUNO" );
            case sym::Op::FpTrue:
                UNREACHABLE_F( "unknown binary operation FpTrue" );
            default:
                UNREACHABLE_F( "unknown binary operation %d %d", bin.op, int( sym::Op::EQ ) );
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
            case sym::Op::Xor:
            case sym::Op::Sub:
                return a != b;
            case sym::Op::Add:
                return a && b;
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
                return a || b;
            case sym::Op::And:
                return a && b;
            case sym::Op::UGE:
            case sym::Op::SLE:
                return a || !b;
            case sym::Op::ULE:
            case sym::Op::SGE:
                return b || !a;
            case sym::Op::UGT:
            case sym::Op::SLT:
                return a && !b;
            case sym::Op::ULT:
            case sym::Op::SGT:
                return b && !a;
            case sym::Op::EQ:
                return a == b;
            case sym::Op::NE:
                return a != b;
            default:
                UNREACHABLE_F( "unknown boolean binary operation %d", bin.op );
        }
    }
}


} // namespace divine::smtl::builder
#endif
#endif
