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

template< typename P, typename H >
bool Context< P, H >::enter_debug()
{
    if ( !debug_allowed() )
        -- _instruction_counter; /* dbg.call does not count */

    if ( debug_allowed() && !debug_mode() )
    {
        -- _instruction_counter;
        ASSERT_EQ( _debug_depth, 0 );
        _debug_reg = _reg;
        _reg[ _VM_CR_Flags ].integer |= _VM_CF_DebugMode;
        with_snap( [&]( auto &h ) { _debug_snap = h.snapshot(); } );
        return true;
    }
    else
        return false;
}

template< typename P, typename H >
void Context< P, H >::leave_debug()
{
    ASSERT( debug_allowed() );
    ASSERT( debug_mode() );
    ASSERT( !_debug_depth );
    _reg = _debug_reg;
    if ( _debug_persist.empty() )
        with_snap( [&]( auto &h ) { h.restore( _debug_snap ); } );
    else
    {
        Heap from = heap();
        with_snap( [&]( auto &h ) { h.restore( _debug_snap ); } );
        for ( auto ptr : _debug_persist )
        {
            if ( !from.valid( ptr ) ) continue;
            heap().free( ptr );
            auto res = heap().make( from.size( ptr ), ptr.object(), true ).cooked();
            ASSERT_EQ( res.object(), ptr.object() );
            heap().copy( from, ptr, ptr, from.size( ptr ) );
        }
        _debug_persist.clear();
    }
}

template< typename P, typename H >
void Context< P, H >::trace( TraceLeakCheck )
{
    bool flagged = false;
    auto leak = [&]( HeapPointer ptr )
    {
        if ( GenericPointer( ptr ) == get( _VM_CR_Constants ).pointer )
            return;
        if ( program().metadata_ptr.count( ptr ) )
            return;
        if ( !flagged )
            fault( _VM_F_Leak, get( _VM_CR_Frame ).pointer, get( _VM_CR_PC ).pointer );
        flagged = true;
        trace( "LEAK: " + brick::string::fmt( ptr ) );
    };

    if ( debug_mode() )
        return;

    /* FIXME 'are we in a fault handler?' duplicated with Eval::fault() */
    PointerV frame( get( _VM_CR_Frame ).pointer ), fpc;
    while ( !frame.cooked().null() )
    {
        heap().read_shift( frame, fpc );
        if ( fpc.cooked().object() == get( _VM_CR_FaultHandler ).pointer.object() )
            return; /* already handling a faut */
        heap().read( frame.cooked(), frame );
    }

    mem::leaked( heap(), leak, get( _VM_CR_State ).pointer, get( _VM_CR_Frame ).pointer );
}

}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
