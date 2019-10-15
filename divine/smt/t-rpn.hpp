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
#include <brick-smt>

#include <cassert>
#include <algorithm>

namespace divine::t_rpn
{
    using namespace divine::smt;
    using namespace brick::smt;
    using RPN = brick::smt::RPN< std::vector< uint8_t > >;
    using RPNView = brick::smt::RPNView< RPN >;

    bool operator==( RPN::Constant< uint64_t > c1, RPNView::Constant c2 )
    {
        return c1.value == c2.value && c1.bitwidth() == c2.bitwidth();
    }

    struct RPN_decomposition
    {
        RPN rpn;

        // dummy first byte, identifying the abstract domain type
        RPN_decomposition() { append( char( 0 ) ); }

        using Constant = RPN::Constant< uint64_t >;   // RPNView::Constant's value is always uint64_t
        using Variable = RPN::Variable;

        template < typename T >
        void append( T t )
        {
            rpn.insert( rpn.end(), bytes_begin( t ), bytes_end( t ) );
        }

        Variable add_var( RPN::VarID id )
        {
            Variable v{ Op::VarI32, id };
            append( v );
            return v;
        }

        Constant add_con( uint64_t val )
        {
            Constant c{ Op( -64 ), val };
            append( c );
            return c;
        }

        template < typename T >
        void check_view( RPNView& view, RPNView::Iterator& it, T val )
        {
            assert( it != view.end() );
            ASSERT_EQ( std::get< T >( *(it) ), val );
        }

        TEST( variable )
        {
            Variable v = add_var( 1 );
            RPNView view( rpn );
            auto it = view.begin();
            check_view< RPNView::Variable >( view, it, v );
            assert( ++it == view.end() );
        }

        TEST( constant )
        {
            Constant c = add_con( 4 );
            RPNView view( rpn );
            auto it = view.begin();
            check_view< RPNView::Constant >( view, it, c );
            assert( ++it == view.end() );
        }

        TEST( trivial_decomp )
        {
            Variable v{ Op::VarI32, 1 };   // x
            append( v );
            auto uf = decompose( rpn );
            ASSERT_EQ( v.id, *uf.find( v.id ) );
        }

        TEST( x_y )
        {
            Variable x{ Op::VarI32, 1 };
            append( x );
            Constant c{ Op( -64 ), 3 };
            append( c );
            append( Op::Eq );
            auto uf = decompose( rpn );
            ASSERT_EQ( x.id, *uf.find( x.id ) );

            Variable y{ Op::VarI32, 2 };
            append( y );
            Constant c2{ Op( -64 ), 4 };
            append( c2 );
            append( Op::Eq );
            append( Op::And );
            auto uf2 = decompose( rpn );
            ASSERT_NEQ( x.id, y.id );
            ASSERT_EQ( *uf2.find( x.id ), *uf2.find( y.id ) );
        }

        // &&C = constraint
        TEST( x_y_z )
        {
            Variable b{ Op::VarBool, 1 };
            append( b );
            Variable z{ Op::VarI32, 2 };       //  ( z=z ) &&C TRUE
            append( z );
            append( z );
            append( Op::Eq );
            append( Op::Constraint );

            Variable x{ Op::VarI32, 3 };       //  &&C ( x=x ) 
            append( x );
            append( x );
            append( Op::Eq );
            append( Op::Constraint );

            Variable y{ Op::VarI32, 4 };
            append( x );
            append( y );                       //  &&C ( y=x )
            append( Op::Eq );
            append( Op::Constraint );

            // expecting partitions {x,y}{z}
            auto uf = decompose( rpn );
            ASSERT_NEQ( x.id, y.id );
            ASSERT_EQ( *uf.find( x.id ), *uf.find( y.id ) );
            ASSERT_NEQ( z.id, x.id );
            ASSERT_NEQ( z.id, y.id );
            ASSERT_NEQ( *uf.find( x.id ), *uf.find( z.id ) );
        }

        TEST( match_variables )
        {
            Variable b{ Op::VarBool, 1 };
            Variable x{ Op::VarI32, 2 };
            Variable y{ Op::VarI32, 3 };
            Variable z{ Op::VarI32, 4 };
            Variable d{ Op::VarI32, 5 };
            Variable e{ Op::VarI32, 6 };
            Constant c{ Op( -64 ), 3 };

            append( b );
            // (x = x)
            append( x );
            append( x );
            append( Op::Eq );
            append( Op::Constraint );

            // (z = z)
            append( z );
            append( z );
            append( Op::Eq );
            append( Op::Constraint );

            // (x = y)
            append( x );
            append( y );
            append( Op::Eq );
            append( Op::Constraint );

            // (z < d)
            append( z );
            append( d );
            append( Op::BvULT );
            append( Op::Constraint );

            // (e = 3)
            append( e );
            append( c );
            append( Op::Eq );
            append( Op::Constraint );

            // expecting partitions {x,y}{z,d}{e}
            auto uf = decompose( rpn );
            ASSERT_EQ( *uf.find( x.id ), *uf.find( y.id ) );
            ASSERT_EQ( *uf.find( z.id ), *uf.find( d.id ) );
            ASSERT_EQ( e.id, *uf.find( e.id ) );

            ASSERT_NEQ( *uf.find( x.id ), *uf.find( z.id ) );
            ASSERT_NEQ( *uf.find( x.id ), *uf.find( e.id ) );
            ASSERT_NEQ( *uf.find( z.id ), *uf.find( e.id ) );
        }
    };
}
