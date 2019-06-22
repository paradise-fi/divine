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
    template< typename next >
    struct ptr2i_i : next
    {
        using Heap = typename next::Heap;
        using HeapInternal = typename Heap::Internal;
        using Snapshot = typename Heap::Snapshot;

        static_assert( !next::uses_ptr2i );
        std::array< HeapInternal, _VM_CR_PtrCount > _ptr2i;

        using next::set;

        void set( _VM_ControlRegister r, GenericPointer v )
        {
            if ( r < _VM_CR_PtrCount )
                _ptr2i[ r ] = v.null() ? HeapInternal() : this->heap().ptr2i( v );
            next::set( r, v );
        }

        HeapInternal ptr2i( Location l )
        {
            ASSERT_LT( l, _VM_Operand::Invalid );
            ASSERT_EQ( next::ptr2i( l ), _ptr2i[ l ] );
            return _ptr2i[ l ];
        }

        void ptr2i( Location l, HeapInternal i )
        {
            if ( i )
                _ptr2i[ l ] = i;
            else
                flush_ptr2i();
        }

        void flush_ptr2i()
        {
            for ( int i = 0; i < _VM_CR_PtrCount; ++i )
                _ptr2i[ i ] = next::ptr2i( Location( i ) );
        }

        auto sync_pc()
        {
            auto newfr = next::sync_pc();
            ptr2i( Location( _VM_CR_Frame ), newfr );
            return newfr;
        }

        void load_pc()
        {
            PointerV pc;
            auto loc = this->heap().loc( this->frame(), ptr2i( Location( _VM_CR_Frame ) ) );
            this->heap().read( loc, pc );
            this->_pc = pc.cooked();
        }
    };

    template< typename next >
    struct snapshot_i : next
    {
        using Heap = typename next::Heap;
        using Snapshot = typename Heap::Snapshot;

        static constexpr const bool uses_ptr2i = true;

        Snapshot snapshot( typename Heap::Pool &p )
        {
            this->sync_pc();
            auto rv = this->heap().snapshot( p );
            this->flush_ptr2i();
            return rv;
        }

        using next::trace;
        virtual void trace( TraceText tt ) { trace( this->heap().read_string( tt.text ) ); }
    };

    using ptr2i = m< ptr2i_i >;
    using snapshot = m< snapshot_i >;
}
