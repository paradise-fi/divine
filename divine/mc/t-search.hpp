// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018-2019 Petr Ročkai <code@fixp.eu>
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

#include <brick-hashset>
#include <random>
#include <set>
#include <divine/mc/search.hpp>

namespace divine::t_mc
{
    enum Label { Red, Black };

    using TQ     = mc::GraphTQ< int, Label >;
    using Edge   = mc::task::Edge< int, Label >;
    using Expand = mc::task::Expand< int >;


    struct Base : TQ::skeleton
    {
        brq::concurrent_hash_set< int > _states;

        using TQ::skeleton::run;

        void run( TQ &tq, mc::task::start )
        {
            _states.insert( 1 );
            tq.add< Expand >( 1 );
        }
    };

    struct Fixed : Base
    {
        std::set< std::pair< int, int > > _edges;

        Fixed( std::initializer_list< std::pair< int, int > > il )
        {
            for ( auto e : il )
                _edges.insert( e );
        }

        Fixed( std::vector< std::pair< int, int > > il )
        {
            for ( auto e : il )
                _edges.insert( e );
        }

        using Base::run;

        void run( TQ &tq, Expand expand )
        {
            for ( auto e : _edges )
                if ( e.first == expand.from )
                {
                    auto r = _states.insert( e.second );
                    tq.add< Edge >( expand.from, e.second, Black, r.isnew() );
                }
        }
    };

    struct Random : Base
    {
        std::vector< std::vector< int > > _succs;

        Random( int vertices, int edges, unsigned seed = 0 )
        {
            std::mt19937 rand{ seed };
            std::uniform_int_distribution< int > dist( 1, vertices );
            _succs.resize( vertices + 1 );
            std::set< int > connected;
            int last = 1;

            /* first connect everything */
            while ( int( connected.size() ) < vertices ) {
                int next = dist( rand );
                if ( connected.count( next ) )
                    continue;
                _succs[ last ].push_back( next );
                connected.insert( next );
                last = next;
                -- edges;
            }

            /* add more edges at random */
            while ( edges > 0 ) {
            next:
                int from = dist( rand );
                int to = dist( rand );
                for ( auto c : _succs[ from ] )
                    if ( to == c )
                        goto next;
                _succs[ from ].push_back( to );
                -- edges;
            }
        }

        using Base::run;

        void run( TQ &tq, Expand e )
        {
            for ( auto t : _succs[ e.from ] )
            {
                auto r = _states.insert( t );
                tq.add< Edge >( e.from, t, Black, r.isnew() );
            }
        }
    };

#ifdef __divine__
    static const int N = 4;
#else
    static const int N = 1000;
#endif

    struct Search
    {
        template< typename B >
        auto weave( B builder )
        {
            return mc::Weaver< TQ >().extend( mc::Search< int, Label >(), builder );
        }

        TEST( fixed )
        {
            Fixed builder{ { 1, 2 }, { 2, 3 }, { 1, 3 }, { 3, 4 } };
            int edgecount = 0, statecount = 0;

            auto edge = [&] ( Edge e )
            {
                if ( e.from == 1 )
                    ASSERT( e.to == 2 || e.to == 3 );
                if ( e.from == 2 )
                    ASSERT_EQ( e.to, 3 );
                if ( e.from == 3 )
                    ASSERT_EQ( e.to, 4 );
                ++ edgecount;
            };
            auto state = [&] ( Expand ) { ++ statecount; };
            weave( builder ).observe( state, edge ).start();

            ASSERT_EQ( edgecount, 4 );
            ASSERT_EQ( statecount, 4 );
        }

        TEST( random )
        {
            for ( unsigned seed = 0; seed < 10; ++ seed )
            {
                Random builder{ 50, 120, seed };
                std::atomic< int > edgecount( 0 ), statecount( 0 );
                auto state = [&]( Edge ) { ++ edgecount; };
                auto edge = [&] ( Expand ) { ++ statecount; };
                weave( builder ).observe( state, edge ).start();

                ASSERT_EQ( statecount.load(), 50 );
                ASSERT_EQ( edgecount.load(), 120 );
            }
        }

        TEST( sequence )
        {
            std::vector< std::pair< int, int > > vec;
            for ( int i = 1; i <= N; ++i )
                vec.emplace_back( i, i + 1 );
            Fixed builder( vec );
            int found = 0;

            weave( builder ).observe( [&]( Expand ) { ++found; } ).start();
            ASSERT_EQ( found, N + 1 );
        }

        TEST( navigate )
        {
            std::vector< std::pair< int, int > > vec;
            for ( int i = 1; i <= N; ++i )
            {
                vec.emplace_back( i, i + 1 );
                vec.emplace_back( i, i + N + 2 );
                vec.emplace_back( i + N + 2, i + 1 );
            }

            Fixed builder( vec );
            int found = 0;

            auto count = [&]( Expand ) { ++found; };
            auto steer = [&]( TQ &tq, Edge e )
            {
                if ( e.from > 1000 )
                    return tq.add< Expand >( e.to );
                if ( ( e.from % 2 == 1 && e.to > 1000 ) || ( e.from % 2 == 0 && e.to < 1000 ) )
                    return tq.add< Expand >( e.to );
            };

            mc::Weaver< TQ >().extend( builder ).extend_f( steer ).observe( count ).start();
            ASSERT_EQ( found, int( 1.5 * N ) );
        }

    };

}
