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
#include <divine/mc/weaver.hpp>

namespace divine::mc::task
{
    template< typename State, typename Label >
    struct Edge
    {
        State from, to;
        Label label;
        bool isnew;
        Edge( State f, State t, Label l, bool n )
            : from( f ), to( t ), label( l ), isnew( n )
        {}
    };

    template< typename State >
    struct Expand
    {
        State from;
        Expand( State s ) : from( s ) {}
    };
}

namespace divine::mc
{
    template< typename State, typename Label >
    using GraphTQ = task_queue< task::Start, task::Expand< State >, task::Edge< State, Label > >;

    template< typename State, typename Label >
    struct Search : GraphTQ< State, Label >::skeleton
    {
        using TQ     = GraphTQ< State, Label >;
        using Edge   = task::Edge< State, Label >;
        using Expand = task::Expand< State >;

        using TQ::skeleton::run;

        void run( TQ &tq, Edge e )
        {
            if ( e.isnew )
                tq.template add< Expand >( e.to );
        }
    };
}
