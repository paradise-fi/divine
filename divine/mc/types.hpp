// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/types.hpp>
#include <optional>

namespace divine::mc
{

    struct BitCode;
    enum class Result { None, Valid, Error, BootError };

    static std::ostream &operator<<( std::ostream &o, Result r )
    {
        switch ( r )
        {
            case Result::None: return o << "error found: unknown";
            case Result::Valid: return o << "error found: no";
            case Result::Error: return o << "error found: yes";
            case Result::BootError: return o << "error found: boot";
        }
    }

    struct Trace
    {
        std::vector< vm::Step > steps;
        std::vector< std::string > labels;
        std::string bootinfo;
        vm::mem::CowSnapshot final;
    };

    template< typename Ex >
    using StateTrace = std::deque< std::pair< vm::mem::CowSnapshot,
                                              std::optional< typename Ex::Label > > >;

    using PoolStats = std::map< std::string, brick::mem::Stats >;
}
