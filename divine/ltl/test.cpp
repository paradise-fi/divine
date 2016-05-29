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


#define BRICK_UNITTEST_REG
#include "brick-assert"
#include "brick-unittest"
#include "ltl.hpp"

namespace brick
{
    namespace unittest
    {
        std::vector< TestCaseBase * > *testcases;
    }
}

struct Test
{
    /*
     *   Bool Tests
     */

    TEST(atomic_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto neg_a = LTL::make( Unary::Neg, atomic_a );

        ASSERT_EQ( atomic_a->string(), "a" );
        ASSERT_EQ( neg_a->string(), "!a" );
    }

    TEST(and_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );

        ASSERT_EQ( and_ab->string(), "( a && b )" );
    }

    TEST(or_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto or_ab = LTL::make( Binary::Or, atomic_a, atomic_b );

        ASSERT_EQ( or_ab->string(), "( a || b )" );
    }

    TEST(impl_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );
        auto impl_ab = LTL::make( Binary::Impl, and_ab, atomic_b );

        ASSERT_EQ( impl_ab->string(), "( !( a && b ) || b )" );
    }

    TEST(equiv_bool)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );
        auto or_ba = LTL::make( Binary::Or, atomic_b, atomic_a );
        auto equiv_ab_ba = LTL::make( Binary::Equiv, and_ab, or_ba );

        ASSERT_EQ( equiv_ab_ba->string(), "( ( a && b ) == ( b || a ) )" );
    }

    /*
     *   LTL Tests
     */

    TEST(atomic_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto neg_l = LTL::make( Unary::Neg, atomic_l );

        ASSERT_EQ( atomic_l->string(), "l" );
        ASSERT_EQ( neg_l->string(), "!l" );
    }

    TEST(until_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto atomic_k = LTL::make( "k" );
        auto until_kl = LTL::make( Binary::Until, atomic_l, atomic_k );

        ASSERT_EQ( until_kl->string(), "( l U k )" );
    }

    TEST(global_until_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto atomic_k = LTL::make( "k" );
        auto until_kl = LTL::make( Binary::Until, atomic_l, atomic_k );
        auto global_kul = LTL::make( Unary::Global, until_kl);

        ASSERT_EQ( global_kul->string(), "G( l U k )" );
    }

    TEST(global_future_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto future_l = LTL::make( Unary::Future, atomic_l );
        auto global_l = LTL::make( Unary::Global, atomic_l );

        auto future_gl = LTL::make( Unary::Future, global_l );
        auto global_fl = LTL::make( Unary::Global, future_l );

        ASSERT_EQ( global_fl->string(), "GFl" );
        ASSERT_EQ( future_gl->string(), "FGl" );
    }

    TEST(until_global_future_LTL)
    {
        auto atomic_l = LTL::make( "l" );
        auto future_l = LTL::make( Unary::Future, atomic_l );
        auto global_l = LTL::make( Unary::Global, atomic_l );

        auto future_gl = LTL::make( Unary::Future, global_l );
        auto global_fl = LTL::make( Unary::Global, future_l );

        auto until_gfl_fgl = LTL::make( Binary::Until, global_fl, future_gl );

        ASSERT_EQ( until_gfl_fgl->string(), "( GFl U FGl )" );
    }

    TEST(until_and_LTL)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );

        auto atomic_l = LTL::make( Binary::Until, atomic_a, and_ab );

        ASSERT_EQ( atomic_l->string(), "( a U ( a && b ) )" );
    }

    TEST(global_or_LTL)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto or_ab = LTL::make( Binary::Or, atomic_a, atomic_b );

        auto atomic_l = LTL::make( Unary::Global, or_ab );

        ASSERT_EQ( atomic_l->string(), "G( a || b )" );
    }

    /*
     *   Normal Form Tests
     */
    
    TEST(atomic_NF)
    {
        auto atomic_a = LTL::make( "a" );
        ASSERT_EQ( atomic_a->string(), atomic_a -> normalForm(  false )->string() );
    }

    TEST(neg_and_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );
        auto neg_and_ab = LTL::make( Unary::Neg, and_ab );

        ASSERT_EQ( neg_and_ab->string(), "!( a && b )" );
        ASSERT_EQ( neg_and_ab->normalForm()->string(), "( !a || !b )" );
        ASSERT_EQ( and_ab->normalForm( true )->string(), "( !a || !b )" );
    }

    TEST(next_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto next_a = LTL::make( Unary::Next, atomic_a );

        ASSERT_EQ( next_a->normalForm()->string(), "Xa" );
    }

    TEST(future_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto future_a = LTL::make( Unary::Future, atomic_a );

        ASSERT_EQ( future_a->normalForm()->string(), "( true U a )" );
    }

    TEST(global_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto global_a = LTL::make( Unary::Global, atomic_a );

        ASSERT_EQ( global_a->normalForm()->string(), "( false R a )" );
    }

    TEST(until_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto until_l = LTL::make( Binary::Until, atomic_a, atomic_b );

        ASSERT_EQ( until_l->normalForm()->string(), "( a U b )" );
        ASSERT_EQ( until_l->normalForm( true )->string(), "( !a R !b )" );
    }

    TEST(neg_until_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );
        auto and_ab = LTL::make( Binary::And, atomic_a, atomic_b );

        auto until_l = LTL::make( Binary::Until, atomic_a, and_ab );
        auto neg_l = LTL::make( Unary::Neg, until_l );

        ASSERT_EQ( neg_l->string(), "!( a U ( a && b ) )" );
        ASSERT_EQ( neg_l->normalForm( false)->string(), "( !a R ( !a || !b ) )" );
    }

    TEST(future_global_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto global_a = LTL::make( Unary::Global, atomic_a );
        auto future_ga = LTL::make( Unary::Future, global_a );

        ASSERT_EQ( future_ga->normalForm()->string(), "( true U ( false R a ) )" );
    }

    TEST(neg_global_future_NF)
    {
        auto atomic_a = LTL::make( "a" );
        auto future_a = LTL::make( Unary::Future, atomic_a );
        auto global_fa = LTL::make( Unary::Global, future_a );
        auto neg_gfa = LTL::make( Unary::Neg, global_fa );

        ASSERT_EQ( neg_gfa->normalForm()->string(), "( true U ( false R !a ) )" );
    }

    TEST(neg_a_until_gb)
    {
        auto atomic_a = LTL::make( "a" );
        auto atomic_b = LTL::make( "b" );

        auto global_b = LTL::make( Unary::Global, atomic_b );
        auto until_a_gb = LTL::make( Binary::Until, atomic_a, global_b );
        auto neg_augb = LTL::make( Unary::Neg, until_a_gb );

        ASSERT_EQ( neg_augb->normalForm()->string(), "( !a R ( true U !b ) )" );
    }
};

int main( int argc, const char **argv ) {
    int r = 1;
    r = brick::unittest::run( argc, argv );
    return r;
}


