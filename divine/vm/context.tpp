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

#include <divine/vm/context.hpp>
#include <divine/vm/memory.tpp>
#include <divine/vm/program.hpp>

namespace divine::vm
{

template< typename H >
void TracingContext< H >::debug_save()
{
    if ( this->heap().can_snapshot() )
    {
        _debug_snap = this->heap().snapshot();
        this->flush_ptr2i();
    }
}

template< typename H >
void TracingContext< H >::debug_restore()
{
    if ( !this->heap().can_snapshot() )
        return;

    if ( _debug_persist.empty() )
    {
        this->heap().restore( _debug_snap );
    }
    else
    {
        auto from = this->heap();
        this->heap().restore( _debug_snap );
        for ( auto ptr : _debug_persist )
        {
            if ( !from.valid( ptr ) ) continue;
            this->heap().free( ptr );
            auto res = this->heap().make( from.size( ptr ), ptr.object(), true ).cooked();
            ASSERT_EQ( res.object(), ptr.object() );
            this->heap().copy( from, ptr, ptr, from.size( ptr ) );
        }
        _debug_persist.clear();
    }
    this->flush_ptr2i();
}

template< typename P, typename H >
void Context< P, H >::trace( TraceLeakCheck )
{
    bool flagged = false;
    auto leak = [&]( HeapPointer ptr )
    {
        if ( ptr == this->constants() )
            return;
        if ( program().metadata_ptr.count( ptr ) )
            return;
        if ( !flagged )
            fault( _VM_F_Leak, this->frame(), this->pc() );
        flagged = true;
        trace( "LEAK: " + brick::string::fmt( ptr ) );
    };

    if ( this->debug_mode() )
        return;

    /* FIXME 'are we in a fault handler?' duplicated with Eval::fault() */
    PointerV frame( this->frame() ), fpc;
    while ( !frame.cooked().null() )
    {
        this->heap().read_shift( frame, fpc );
        if ( fpc.cooked().object() == this->fault_handler().object() )
            return; /* already handling a faut */
        this->heap().read( frame.cooked(), frame );
    }

    mem::leaked( this->heap(), leak, this->state_ptr(), Base::frame(), Base::globals() );
}

}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
