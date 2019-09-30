
// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/mc/safety.hpp>
#include <divine/mc/job.tpp>
#include <divine/mc/t-builder.hpp>

namespace divine::t_mc
{
    struct TestSafety
    {
        TEST( simple )
        {
            auto bc = prog_int( "4", "*r - 1" );
            int edgecount = 0, statecount = 0;
            auto safe = mc::make_job< mc::Safety >(
                bc, ss::passive_listen(
                    [&]( auto, auto, auto ) { ++edgecount; },
                    [&]( auto ) { ++statecount; } ) );
            safe->start( 1 );
            safe->wait();
            ASSERT_EQ( edgecount, 4 );
            ASSERT_EQ( statecount, 5 );
        }
    };
}
