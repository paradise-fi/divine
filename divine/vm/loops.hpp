// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/pointer.hpp>
#include <unordered_set>

namespace divine::vm
{
    struct track_loops
    {
        std::vector< std::unordered_set< GenericPointer > > loops;

        void entered( CodePointer )
        {
            loops.emplace_back();
        }

        void left( CodePointer )
        {
            loops.pop_back();
            if ( loops.empty() ) /* more returns than calls could happen along an edge */
                loops.emplace_back();
        }

        bool test_loop( CodePointer pc, int /* TODO */ )
        {
            /* TODO track the counter value too */
            if ( loops.back().count( pc ) )
                return true;
            else
                loops.back().insert( pc );

            return false;
        }
    };
}
