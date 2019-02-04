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

namespace divine::t_mc
{

    struct Tasks
    {
        struct T1
        {
            int i;
            T1( int i ) : i( i ) {}
        };

        struct T2
        {
            int j;
            T2( int j ) : j( j ) {}
        };
    };

    using TQ = mc::TaskQueue< Tasks, Tasks::T1, Tasks::T2 >;

    struct Machine1 : TQ::Skel
    {
        using TQ::Skel::run;
        void run( TQ &tq, Tasks::T1 t )
        {
            if ( t.i < 10 )
                tq.add< T2 >( t.i + 1 );
        }
    };

    struct Machine2 : TQ::Skel
    {
        using TQ::Skel::run;
        void run( TQ &tq, T2 t )
        {
            tq.add< T1 >( t.j + 1 );
        }
    };

    struct Counter : Tasks
    {
        int t1 = 0, t2 = 0;
        void run( TQ &, T1 ) { ++t1; }
        void run( TQ &, T2 ) { ++t2; }
    };

    struct Weave
    {
        TEST( basic )
        {
            mc::Weaver< TQ, Machine1, Machine2, Counter > weaver;
            weaver.add< Tasks::T1 >( 3 );
            weaver.run();
            auto &ctr = weaver.machine< Counter >();
            ASSERT_EQ( ctr.t1, 5 );
            ASSERT_EQ( ctr.t2, 4 );
        }
    };
}
