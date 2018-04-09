// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Vladimír Štill
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

#include <string>
#include <optional>
#include <external/nanodbc/nanodbc.h>

namespace benchmark
{

struct Config
{
    std::optional< std::string > solver;
    std::optional< int > threads;
    std::optional< int64_t > max_mem;
    std::optional< int > max_time;
};

inline Config get_config( nanodbc::connection &conn, int config_id )
{
    nanodbc::statement q( conn, "select solver, threads, max_mem, max_time from config where id = ?" );
    q.bind( 0, &config_id );
    auto r = q.execute();
    r.first();
    auto solver = r.get< std::string >( 0 ); /* text fields and nulls don't go together */

    Config c;
    if ( !solver.empty() ) c.solver   = solver;
    if ( !r.is_null( 1 ) ) c.threads  = r.get< int >( 1 );
    if ( !r.is_null( 2 ) ) c.max_mem  = r.get< int64_t >( 2 );
    if ( !r.is_null( 3 ) ) c.max_time = r.get< int >( 3 );
    return c;
}

}
