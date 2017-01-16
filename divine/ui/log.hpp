// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/mc/job.hpp>
#include <brick-string>

namespace divine {
namespace ui {

struct LogSink
{
    virtual void progress( int, int, bool ) {}
    virtual void memory( const mc::Job::PoolStats &, bool ) {}
    virtual void info( std::string ) {}
    virtual void result( mc::Result, const mc::Trace & ) {}
    virtual void start() {}
    virtual ~LogSink() {}
};

using SinkPtr = std::shared_ptr< LogSink >;

SinkPtr nullsink(); /* a global null sink */
SinkPtr make_interactive();
SinkPtr make_yaml( bool detailed );
SinkPtr make_odbc( std::string connstr );
SinkPtr make_composite( std::vector< SinkPtr > );

}
}
