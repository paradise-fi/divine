/*
 * (c) 2016 Viktória Vozárová <>
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

#include <iostream>
#include <brick-types>
#include <cassert>
#include <set>
#include <vector>

#ifndef LTL2C_LTL_H
#define LTL2C_LTL_H

namespace divine {
namespace ltl {

struct LTL;

using LTLPtr = std::shared_ptr< LTL >;
struct LTLComparator
{
    // @return true iff ltlA < ltlB lexicographically on strings or if equal strings but smaller U parents
    bool operator()( LTLPtr ltlA, LTLPtr ltlB ) const;
};

struct LTLComparator2
{
    // @return true iff ltlA < ltlB lexicographically on strings
    bool operator()( LTLPtr ltlA, LTLPtr ltlB ) const;
};


struct Atom
{
    std::string label;
    std::string string() const { return label; }
    LTLPtr normalForm( bool );
    LTLPtr normalForm();

    bool isComplete() const { return true; }
};

struct Unary
{
    enum Operator { Neg, Global, Future, Next  }  op;
    LTLPtr subExp;

    std::string string() const;
    LTLPtr normalForm( bool );
    LTLPtr normalForm() { return normalForm( false ); }

    bool isComplete() const { return subExp != nullptr; }
};

struct Binary
{
    enum Operator { And, Or, Impl, Equiv, Until, Release } op;
    LTLPtr left, right;

    std::string string() const;
    LTLPtr normalForm( bool );
    LTLPtr normalForm() { return normalForm( false ); }

    bool isComplete() const { return left != nullptr && right != nullptr; }
};

using Exp = brick::types::Union< Atom, Unary, Binary >;
struct LTL : Exp
{
    using Exp::Union;

    struct Comparator
    {
        bool operator()( LTLPtr ltlA, LTLPtr ltlB ) const
        {
            return ltlA->string() == ltlB->string();
        }
    };

    std::string string()
    {
        return apply( [] ( auto e ) -> std::string { return e.string(); } ).value();
    }

    static LTLPtr parse( const std::string& str );

    bool isComplete()
    {
        return apply([]( auto e ) -> bool { return e.isComplete(); } ).value();
    }

    LTLPtr normalForm()
    {
        return normalForm( false );
    }

    LTLPtr normalForm( bool neg )
    {
        return apply( [=]( auto e ) -> LTLPtr { return e.normalForm( neg ); } ).value();
    }

    static LTLPtr make( Binary::Operator op, LTLPtr left = nullptr, LTLPtr right = nullptr )
    {
        Binary binary;
        binary.op = op;
        binary.left = left;
        binary.right = right;
        return std::make_shared< LTL >( binary );
    }

    static LTLPtr make( Unary::Operator op, LTLPtr subExp = nullptr )
    {
        Unary unary;
        unary.op = op;
        unary.subExp = subExp;
        return std::make_shared< LTL >( unary );
    }

    static LTLPtr make( const std::string & label )
    {
        Atom atom;
        atom.label = label;
        return std::make_shared< LTL >( atom );
    }

    static LTLPtr make( LTL ltl )
    {
        return std::make_shared< LTL >( ltl );
    }
};

} // end of namespace ltl

namespace t_ltl {

using namespace ltl;

static void check(LTLPtr f, std::string exp)
{
    ASSERT_EQ( f->string(), exp );
    ASSERT_EQ( LTL::parse( exp )->string(), exp );
}

struct LTLToString
{
    /*
     *   Bool Tests
     */

    TEST(atomic_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto neg_a = LTL::make( Unary::Neg, atomic_a );

        check( atomic_a, "a" );
        check( neg_a, "!a" );
    }

    TEST(and_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );

        check( and_ab, "( a & b )" );
    }

    TEST(or_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto or_ab = LTL::make( Binary::Or, atomic_a, atomic_b );

        check(or_ab, "( a | b )");
    }

    TEST(impl_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );
        auto impl_ab = LTL::make( Binary::Impl, and_ab, atomic_b );

        check( impl_ab, "( ( a & b ) -> b )" );
    }

    TEST(equiv_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );
        auto or_ba = LTL::make( Binary::Or, atomic_b, atomic_a );
        auto equiv_ab_ba = LTL::make( Binary::Equiv, and_ab, or_ba );

        check( equiv_ab_ba, "( ( a & b ) = ( b | a ) )" );
    }

    /*
     *   LTL Tests
     */

    TEST(atomic_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto neg_l = LTL::make( Unary::Neg, atomic_l );

        check( atomic_l, "l" );
        check( neg_l, "!l" );
    }

    TEST(until_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto atomic_k = LTL::make( "k" );
        auto until_kl = LTL::make( Binary::Until, atomic_l, atomic_k );

        check( until_kl, "( l U k )" );
    }

    TEST(global_until_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto atomic_k = LTL::make( "k" );
        auto until_kl = LTL::make( Binary::Until, atomic_l, atomic_k );
        auto global_kul = LTL::make( Unary::Global, until_kl );

        check( global_kul, "G( l U k )" );
    }

    TEST(global_future_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto future_l = LTL::make( Unary::Future, atomic_l );
        auto global_l = LTL::make( Unary::Global, atomic_l );

        auto future_gl = LTL::make( Unary::Future, global_l );
        auto global_fl = LTL::make( Unary::Global, future_l );

        check( global_fl, "GFl" );
        check( future_gl, "FGl" );
    }

    TEST(until_global_future_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto future_l = LTL::make( Unary::Future, atomic_l );
        auto global_l = LTL::make( Unary::Global, atomic_l );

        auto future_gl = LTL::make( Unary::Future, global_l );
        auto global_fl = LTL::make( Unary::Global, future_l );

        auto until_gfl_fgl = LTL::make( Binary::Until, global_fl, future_gl );

        check( until_gfl_fgl, "( GFl U FGl )" );
    }

    TEST(until_and_LTL)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );

        auto atomic_l = LTL::make( Binary::Until, atomic_a, and_ab );

        check( atomic_l, "( a U ( a & b ) )" );
    }

    TEST(global_or_LTL)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto or_ab = LTL::make( Binary::Or, atomic_a, atomic_b );

        auto atomic_l = LTL::make( Unary::Global, or_ab );

        check( atomic_l, "G( a | b )" );
    }
    /*
    * Complex string -> ltl tests
    */
    TEST(complex_ltl_01)
    {
        std::string str( "G!F( aa & bb | cc) U ( a U b ) R c" );
        LTLPtr ltl = LTL::parse( str );
        ASSERT_EQ( ltl->string(), "( ( G!F( ( aa & bb ) | cc ) U ( a U b ) ) R c )" );
    }

    TEST(asociativity_AND)
    {
        std::string str( "a & b & c" );
        LTLPtr ltl = LTL::parse( str );
        ASSERT_EQ( ltl->string(), "( ( a & b ) & c )" );
    }

    TEST(asociativity_Until)
    {
        std::string str( " a U b U c" );
        LTLPtr ltl = LTL::parse( str );
        ASSERT_EQ( ltl->string(), "( a U ( b U c ) )" );
        check( ltl, ltl->string() );
    }
};

struct LTLNF /* normal forms */
{
    TEST(atomic_NF)
    {
        auto atomic_a = LTL::make( "a" );
        check( atomic_a, atomic_a->normalForm( false )->string() );
    }

    TEST(neg_and_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );
        auto neg_and_ab = LTL::make( Unary::Neg, and_ab );

        check( neg_and_ab, "!( a & b )" );
        check( neg_and_ab->normalForm(), "( !a | !b )" );
        check( and_ab->normalForm(true), "( !a | !b )" );
    }

    TEST(next_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto next_a = LTL::make( Unary::Next, atomic_a );

        check( next_a->normalForm(), "Xa" );
    }

    TEST(future_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto future_a = LTL::make( Unary::Future, atomic_a );

        check( future_a->normalForm(), "( true U a )" );
    }

    TEST(global_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto global_a = LTL::make( Unary::Global, atomic_a );

        check( global_a->normalForm(), "( false R a )" );
    }

    TEST(until_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto until_l = LTL::make( Binary::Until, atomic_a, atomic_b );

        check( until_l->normalForm(), "( a U b )" );
        check( until_l->normalForm(true), "( !a R !b )" );
    }

    TEST(neg_until_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );

        auto until_l = LTL::make( Binary::Until, atomic_a, and_ab );
        auto neg_l = LTL::make( Unary::Neg, until_l );

        check( neg_l, "!( a U ( a & b ) )" );
        check( neg_l->normalForm(false), "( !a R ( !a | !b ) )" );
    }

    TEST(future_global_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto global_a = LTL::make( Unary::Global, atomic_a );
        auto future_ga = LTL::make( Unary::Future, global_a );

        check( future_ga->normalForm(), "( true U ( false R a ) )" );
    }

    TEST(neg_global_future_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto future_a = LTL::make( Unary::Future, atomic_a );
        auto global_fa = LTL::make( Unary::Global, future_a );
        auto neg_gfa = LTL::make( Unary::Neg, global_fa );

        check( neg_gfa->normalForm(), "( true U ( false R !a ) )" );
    }

    TEST(neg_a_until_gb)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );

        auto global_b = LTL::make( Unary::Global, atomic_b );
        auto until_a_gb = LTL::make( Binary::Until, atomic_a, global_b );
        auto neg_augb = LTL::make( Unary::Neg, until_a_gb );

        check( neg_augb->normalForm(), "( !a R ( true U !b ) )" );
    }
};

}
}

#endif
