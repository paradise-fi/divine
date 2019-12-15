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

namespace divine::vm::ctx
{
    /* Implementation of debug mode. Include in the context stack when you want
     * dbg.call to be actually performed. Not including ctx_debug in the context
     * saves considerable resources (especially computation time). */

    template< typename next >
    struct debug_i : next
    {
        State _debug_state;
        PtrRegister _debug_pc;

        int _debug_depth = 0;
        bool _debug_allowed = false;
        std::vector< Interrupt > _interrupts;

        static constexpr const bool uses_ptr2i = true;
        static constexpr const bool has_debug_mode = true;

        bool track_test( Interrupt::Type t, CodePointer pc )
        {
            _interrupts.push_back( Interrupt{ t, this->_state.instruction_counter, pc } );
            return true;
        }

        bool debug_allowed() { return _debug_allowed; }
        bool debug_mode() { return this->flags_any( _VM_CF_DebugMode ); }
        void enable_debug() { _debug_allowed = true; }

        bool test_loop( CodePointer p, int i )
        {
            return !debug_mode() && next::test_loop( p, i );
        }

        bool test_crit( CodePointer p, GenericPointer a, int s, int t )
        {
            return !debug_mode() && next::test_crit( p, a, s, t );
        }

        bool enter_debug();
        void leave_debug();

        void debug_save();
        void debug_restore();

        using next::set;

        void set( _VM_ControlRegister r, uint64_t v )
        {
            next::set( r, v );
            if ( _debug_depth )
                ASSERT( debug_mode() );
        }

        void entered( CodePointer f )
        {
            if ( debug_mode() )
                ++ _debug_depth;
            else
                next::entered( f );
        }

        void left( CodePointer f )
        {
            if ( debug_mode() )
            {
                ASSERT_LEQ( 1, _debug_depth );
                -- _debug_depth;
                if ( !_debug_depth )
                    leave_debug();
            }
            else
                next::left( f );
        }

        std::vector< HeapPointer > _debug_persist;
        typename next::Heap::Pool _debug_pool;
        typename next::Heap::Snapshot _debug_snap;

        using next::trace;

        virtual void trace( TraceDebugPersist t )
        {
            if ( t.ptr.type() == PointerType::Weak )
                _debug_persist.push_back( t.ptr );
            else
            {
                this->trace( brq::format( "FAULT: cannot persist a non-weak object ", t.ptr ).buffer() );
                this->fault( _VM_F_Memory, this->frame(), this->pc() );
            }
        }
    };

    using debug = m< debug_i >;
}
