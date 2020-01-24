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
#include <divine/vm/types.hpp>
#include <divine/vm/pointer.hpp>
#include <divine/smt/extract.hpp>
#include <brick-compose>

namespace divine::mc
{
    template< typename next >
    struct ctx_assume_ : next
    {
        std::vector< vm::HeapPointer > _assume;

        using next::trace;
        void trace( vm::TraceAssume ta )
        {
            _assume.push_back( ta.ptr );
            if ( this->debug_allowed() )
                trace( "ASSUME " + smt::extract::to_string( this->heap(), ta.ptr ) );
        }
    };

    using ctx_assume = brq::module< ctx_assume_ >;
}
