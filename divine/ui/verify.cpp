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

#include <divine/vm/explore.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/vm/debug.hpp>
#include <divine/ss/search.hpp>
#include <divine/ui/cli.hpp>

#include <sys/time.h>

namespace divine {
namespace ui {

void Verify::run()
{
    vm::Explore ex( _bc );
    int edgecount = 0, statecount = 0;
    timeval start, now;
    ::gettimeofday(&start, NULL);

    ss::search( ss::Order::PseudoBFS, ex, 1, ss::passive_listen(
                    [&]( auto, auto, auto )
                    {
                        ++edgecount;
                    },
                    [&]( auto )
                    {
                        ++statecount;
                        if ( statecount % 100 == 0 )
                        {
                            ::gettimeofday(&now, NULL);
                            std::stringstream time;
                            int seconds = now.tv_sec - start.tv_sec;
                            float avg = float( statecount ) / seconds;
                            time << seconds / 60 << ":"
                                 << std::setw( 2 ) << std::setfill( '0' ) << seconds % 60;
                            std::cerr << "\rsearching: " << edgecount << " edges and "
                                      << statecount << " states found in " << time.str() << ", for "
                                      << std::fixed << std::setprecision( 1 ) << avg << " states/s average";
                        }
                    } ) );
    std::cerr << std::endl << "found " << statecount << " states and " << edgecount << " edges" << std::endl;
}

}
}
