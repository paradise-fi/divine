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

#include <divine/vm/ctx-base.hpp>
#include <divine/vm/ctx-track.hpp>
#include <divine/vm/ctx-memory.hpp>
#include <divine/vm/ctx-fault.hpp>
#include <divine/vm/ctx-frame.hpp>
#include <divine/vm/ctx-legacy.hpp>
#include <divine/vm/ctx-debug.hpp>

namespace divine::vm::ctx
{
    template< typename prog, typename heap >
    using common = compose< track_nothing, snapshot, ptr2i, base< prog, heap > >;
    using with_tracking = compose< track_loops, track_nothing >;
    using with_debug = compose< legacy, fault, debug >;
}

namespace divine::vm
{
    using State = ctx::State;

    template< typename prog, typename heap >
    struct Context : compose_stack< ctx::with_debug, ctx::with_tracking, ctx::common< prog, heap > > {};

    template< typename prog, typename heap >
    struct ctx_const : compose_stack< ctx::track_nothing, ctx::common< prog, heap > >
    {
        void setup( int gds, int cds )
        {
            this->set( _VM_CR_Constants, this->heap().make( cds ).cooked() );
            if ( gds )
                this->set( _VM_CR_Globals, this->heap().make( gds ).cooked() );
        }
    };

}
