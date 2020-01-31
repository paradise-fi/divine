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
#include <brick-mem>
#include <brick-hashset>
#include <brick-string>
#include <brick-timer>
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

    static brq::parse_result from_string( std::string_view s, Result &r )
    {
        if      ( s == "valid" ) r = Result::Valid;
        else if ( s == "error" ) r = Result::Error;
        else if ( s == "boot-error" ) r = Result::BootError;
        else return brq::no_parse( "expected 'valid', 'error' or 'boot-error'" );

        return {};
    }

    struct Trace
    {
        std::vector< vm::Step > steps;
        std::vector< std::string > labels;
        std::string bootinfo;
        vm::CowSnapshot final;
    };

    template< typename Label >
    using LabelledTrace = std::deque< std::pair< vm::CowSnapshot,
                                                 std::optional< Label > > >;
    template< typename Ex >
    using StateTrace = LabelledTrace< typename Ex::Label >;

    using PoolStats = std::map< std::string, brick::mem::Stats >;
    using HashStats = std::map< std::string, brq::hash_set_stats >;

    struct divm_timer_tag;
    struct hash_timer_tag;

    using divm_timer = brq::timer< divm_timer_tag >;
    using hash_timer = brq::timer< hash_timer_tag >;
}
