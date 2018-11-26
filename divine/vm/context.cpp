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

#include <divine/vm/program.hpp>
#include <divine/vm/context.hpp>
#include <divine/vm/context.tpp>

namespace divine::vm
{

bool Tracing::enter_debug()
{
    if ( !debug_allowed() )
        -- _state.instruction_counter; /* dbg.call does not count */

    if ( debug_allowed() && !debug_mode() )
    {
        -- _state.instruction_counter;
        ASSERT_EQ( _debug_depth, 0 );
        _debug_state = _state;
        this->flags_set( 0, _VM_CF_DebugMode );
        this->debug_save();
        return true;
    }
    else
        return false;
}

void Tracing::leave_debug()
{
    ASSERT( debug_allowed() );
    ASSERT( debug_mode() );
    ASSERT( !_debug_depth );
    _state = _debug_state;
    this->debug_restore();
}

template struct Context< Program, CowHeap >;
template struct Context< Program, SmallHeap >;
template struct Context< Program, MutableHeap >;

}
