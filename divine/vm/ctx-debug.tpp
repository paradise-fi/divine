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
#include <divine/vm/context.hpp>
#include <divine/vm/memory.tpp>
#include <divine/vm/program.hpp>

namespace divine::vm::ctx
{

    template< typename next >
    void debug_i< next >::debug_save()
    {
        if ( this->heap().can_snapshot() )
        {
            _debug_snap = this->heap().snapshot( _debug_pool );
            this->flush_ptr2i();
        }
    }

    template< typename next >
    void debug_i< next >::debug_restore()
    {
        if ( !this->heap().can_snapshot() )
            return;

        if ( _debug_persist.empty() )
        {
            this->heap().restore( _debug_pool, _debug_snap );
        }
        else
        {
            this->heap().snapshot( _debug_pool );
            auto from = this->heap();
            this->heap().restore( _debug_pool, _debug_snap );
            for ( auto ptr : _debug_persist )
            {
                TRACE( "restoring persisted pointer", ptr );
                this->heap().free( ptr );
                if ( !from.valid( ptr ) ) continue;
                auto res = this->heap().make( from.size( ptr ), ptr.object(), true ).cooked();
                ASSERT_EQ( res.object(), ptr.object() );
                this->heap().copy( from, ptr, ptr, from.size( ptr ) );
            }
            _debug_persist.clear();
        }
        this->flush_ptr2i();
    }

    template< typename next >
    void legacy_i< next >::trace( TraceLeakCheck )
    {
        bool flagged = false;
        auto leak = [&]( HeapPointer ptr )
        {
            if ( ptr == this->constants() )
                return;
            if ( this->program().metadata_ptr.count( ptr ) )
                return;
            if ( !flagged )
                this->fault( _VM_F_Leak, this->frame(), this->pc() );
            flagged = true;
            TRACE( "object", ptr, "leaked" );

            if ( this->_fault.empty() )
                this->_fault += "leaked";

            this->_fault += brq::format( " [", ptr, "]" );
        };

        if ( this->debug_mode() )
            return;

        /* FIXME 'are we in a fault handler?' duplicated with Eval::fault() */
        PointerV frame( this->frame() ), fpc;
        while ( this->heap().valid( frame.cooked() ) )
        {
            this->heap().read_shift( frame, fpc );
            if ( fpc.cooked().object() == this->fault_handler().object() )
                return; /* already handling a faut */
            this->heap().read( frame.cooked(), frame );
        }

        mem::leaked( this->heap(), leak, HeapPointer( this->state_ptr() ),
                     HeapPointer( this->frame() ), HeapPointer( this->globals() ) );
    }

    template< typename next >
    bool debug_i< next >::enter_debug()
    {
        if ( !debug_allowed() )
            -- this->_state.instruction_counter; /* dbg.call does not count */

        if ( debug_allowed() && !debug_mode() )
        {
            TRACE( "entering debug mode" );
            -- this->_state.instruction_counter;
            ASSERT_EQ( _debug_depth, 0 );
            _debug_state = this->_state;
            _debug_pc = this->_pc;
            this->flags_set( 0, _VM_CF_DebugMode );
            this->debug_save();
            return true;
        }
        else
            return false;
    }

    template< typename next >
    void debug_i< next >::leave_debug()
    {
        ASSERT( debug_allowed() );
        ASSERT( debug_mode() );
        ASSERT( !_debug_depth );
        TRACE( "leaving debug mode" );
        this->_state = _debug_state;
        this->_pc = _debug_pc;
        this->debug_restore();
    }
}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
