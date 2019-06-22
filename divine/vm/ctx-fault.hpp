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
    struct fault_i : next
    {
        std::string _fault;

        using next::trace;
        void trace( TraceFault tt ) override { _fault = tt.string; }

        void fault_clear() override { _fault.clear(); }
        std::string fault_str() override { return _fault; }

        void doublefault() override
        {
            this->flags_set( 0, _VM_CF_Error | _VM_CF_Stop );
        }

        void fault( Fault f, HeapPointer frame, CodePointer pc ) override
        {
            auto fh = this->fault_handler();
            if ( this->debug_mode() )
            {
                this->trace( "W: " + this->fault_str() + " in debug mode (abandoned)" );
                this->fault_clear();
                this->_debug_depth = 0; /* short-circuit */
                this->leave_debug();
            }
            else if ( fh.null() )
            {
                this->trace( "FATAL: no fault handler installed" );
                doublefault();
            }
            else
                make_frame( *this, fh, PointerV( this->frame() ),
                            value::Int< 32 >( f ), PointerV( frame ), PointerV( pc ) );
        }
    };

    using fault = m< fault_i >;
}
