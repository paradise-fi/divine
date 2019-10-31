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
#include <brick-data>

namespace divine::vm::ctx
{
    template< typename next >
    struct legacy_i : next
    {
        using Program = typename next::Program;
        using Heap = typename next::Heap;
        using Pool = typename Heap::Pool;
        using PointerV = value::Pointer;
        using Location = typename Program::Slot::Location;
        using Snapshot = typename Heap::Snapshot;
        using next::_heap;

        static constexpr const bool uses_ptr2i = true;

        vm::HeapPointer _constraints;
        bool _track_mem = false;

        using next::trace;
        virtual void trace( TraceLeakCheck ) override;

        void trace( vm::TraceConstraints ta )
        {
            _constraints = ta.ptr;
        }

        vm::HeapPointer constraint_ptr() const
        {
            return _constraints;
        }

        using MemMap = brick::data::IntervalSet< GenericPointer >;

        void track_memory( bool b ) { _track_mem = b; }

        template< typename Ctx >
        void load( const Ctx &ctx )
        {
            clear();
            for ( int i = 0; i < _VM_CR_Last; ++i )
            {
                auto cr = _VM_ControlRegister( i );
                if ( cr == _VM_CR_Flags || cr == _VM_CR_ObjIdShuffle )
                    this->set( cr, ctx.get_int( cr ) );
                else
                    this->set( cr, ctx.get_ptr( cr ) );
            }
            ASSERT( !this->debug_mode() );
            this->_heap = ctx.heap();
            _constraints = ctx.constraint_ptr();
        }

        virtual void clear()
        {
            this->_fault.clear();
            this->_interrupts.clear();
            this->reset_interrupted();
            this->flush_ptr2i();
            this->set( _VM_CR_User1, GenericPointer() );
            this->set( _VM_CR_User2, GenericPointer() );
            this->set( _VM_CR_User3, GenericPointer() );
            this->set( _VM_CR_User4, GenericPointer() );
            this->set( _VM_CR_ObjIdShuffle, 0 );
            this->_state.instruction_counter = 0;
        }

        void load( Pool &p, Snapshot snap ) { _heap.restore( p, snap ); clear(); }
        void reset() { _heap.reset(); clear(); }

        HeapPointer frame() { return next::frame(); }
        HeapPointer globals() { return next::globals(); }
        HeapPointer constants() { return next::constants(); }

        bool test_loop( CodePointer pc, int ctr )
        {
            if ( this->flags_all( _VM_CF_IgnoreLoop ) || this->debug_mode() )
                return false;

            if ( next::test_loop( pc, ctr ) )
                return this->track_test( Interrupt::Cfl, pc );
            else
                return false;
        }

        void count_instruction()
        {
            if ( !this->debug_mode() )
                ++ this->_state.instruction_counter;
        }
    };

    using legacy = m< legacy_i >;
}
