// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Zuzana Baranova <xbaranov@fi.muni.cz>
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

#pragma once

#include <divine/smt/rpn.hpp>
#include <brick-assert>

#include <cassert>
#include <algorithm>

namespace divine::t_smt
{
    using namespace divine::smt;

    struct decomposition
    {
        template< typename T > using stack_t = std::vector< T >;
        template< typename imm_t > using atom_t = brq::smt_atom_t< imm_t >;
        using expr_t = brq::smt_expr< std::vector >;
        using varid_t = brq::smt_varid_t;
        using var_t = atom_t< varid_t >;
        using const_t = atom_t< uint64_t >;
        using op = brq::smt_op;
        using uf_t = brq::union_find< std::map< int, int > >;

        expr_t expr;
        uf_t uf;

        auto decompose() { return brq::smt_decompose< stack_t >( expr, uf ); }
        void clear() { expr.clear(); }

        void add_var( varid_t id )     { expr.apply( var_t( op::var_i32, id ) ); }
        void add_const( uint64_t val ) { expr.apply( const_t( op::const_i64, val ) ); }

        TEST( variable )
        {
            add_var( 1 );
            ASSERT_EQ( expr.begin()->varid(), 1 );
            ASSERT( ++expr.begin() == expr.end() );
        }

        TEST( constant )
        {
            add_const( 4 );
            ASSERT_EQ( expr.begin()->value(), 4 );
            ASSERT( ++expr.begin() == expr.end() );
        }


        TEST( trivial_decomp )
        {
            add_var( 1 );
            decompose();
            ASSERT_EQ( 1, uf.find( 1 ) );
        }

        TEST( x_y )
        {
            add_var( 1 );
            add_const( 3 );
            expr.apply( op::eq );
            decompose();

            ASSERT_EQ( 1, uf.find( 1 ) );

            clear();

            add_var( 2 );
            add_const( 4 );
            expr.apply( op::eq );
            decompose();

            ASSERT_EQ( 1, uf.find( 1 ) );
            ASSERT_EQ( 2, uf.find( 2 ) );
        }

        // &&C = constraint
        TEST( x_y_z )
        {
            add_var( 1 );
            add_var( 2 );
            add_var( 2 );
            expr.apply( op::eq );
            expr.apply( op::bool_and );

            decompose();
            clear();

            add_var( 3 );
            add_var( 3 );
            expr.apply( op::eq );
            decompose();
            clear();

            add_var( 4 );
            add_var( 3 );
            expr.apply( op::eq );
            decompose();

            // expecting partitions { 3, 4 } { 1, 2 }
            ASSERT_EQ( uf.find( 3 ), uf.find( 4 ) );
            ASSERT_EQ( uf.find( 1 ), uf.find( 2 ) );
            ASSERT_NEQ( uf.find( 3 ), uf.find( 2 ) );
            ASSERT_NEQ( uf.find( 3 ), uf.find( 1 ) );
        }

        TEST( match_variables )
        {
            add_var( 2 );
            add_var( 2 );
            expr.apply( op::eq );
            decompose();
            clear();

            add_var( 4 );
            add_var( 4 );
            expr.apply( op::eq );
            decompose();
            clear();

            add_var( 2 );
            add_var( 3 );
            expr.apply( op::eq );
            decompose();
            clear();

            add_var( 4 );
            add_var( 5 );
            expr.apply( op::bv_ult );
            decompose();
            clear();

            add_var( 6 );
            add_const( 3 );
            expr.apply( op::eq );
            decompose();

            // expecting partitions { 2, 3 } { 4, 5 } { 6 }

            ASSERT_EQ( uf.find( 2 ), uf.find( 3 ) );
            ASSERT_EQ( uf.find( 4 ), uf.find( 5 ) );
            ASSERT_EQ( 6, uf.find( 6 ) );

            ASSERT_NEQ( uf.find( 2 ), uf.find( 4 ) );
            ASSERT_NEQ( uf.find( 2 ), uf.find( 6 ) );
            ASSERT_NEQ( uf.find( 4 ), uf.find( 6 ) );
        }
    };
}
