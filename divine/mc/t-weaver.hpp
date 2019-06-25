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

    struct task1
    {
        int i;
        task1( int i ) : i( i ) {}
    };

    struct task2
    {
        int j;
        task2( int j ) : j( j ) {}
    };

    using tq = mc::task_queue< task1, task2 >;

    struct machine1 : tq::skeleton
    {
        using tq::skeleton::run;

        void run( tq &q, task1 t )
        {
            if ( t.i < 10 )
                q.add< task2 >( t.i + 1 );
        }
    };

    struct machine2 : tq::skeleton
    {
        using tq::skeleton::run;

        void run( tq &q, task2 t )
        {
            q.add< task1 >( t.j + 1 );
        }
    };

    struct counter
    {
        int t1 = 0, t2 = 0;
        void run( tq &, task1 ) { ++t1; }
        void run( tq &, task2 ) { ++t2; }
    };

    struct Weave
    {
        TEST( basic )
        {
            mc::Weaver< tq, machine1, machine2, counter > weaver;
            weaver.add< task1 >( 3 );
            weaver.run();
            auto &ctr = weaver.machine< counter >();
            ASSERT_EQ( ctr.t1, 5 );
            ASSERT_EQ( ctr.t2, 4 );
        }
    };
}
