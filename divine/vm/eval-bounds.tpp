// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2019 Petr Ročkai <code@fixp.eu>
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
#include <divine/vm/eval.hpp>

namespace divine::vm
{

    template< typename Ctx > template< typename MkF >
    bool Eval< Ctx >::boundcheck( MkF mkf, PointerV p, int sz, bool write, std::string dsc )
    {
        auto pp = p.cooked();
        int width = 0;

        if ( !p.defined() )
        {
            mkf( _VM_F_Memory ) << "undefined pointer dereference: " << p << dsc;
            return false;
        }

        if ( pp.null() )
        {
            mkf( _VM_F_Memory ) << "null pointer dereference: " << p << dsc;
            return false;
        }

        if ( pp.type() == PointerType::Code )
        {
            mkf( _VM_F_Memory ) << "attempted to dereference a code pointer " << p << dsc;
            return false;
        }

        if ( !p.pointer() )
        {
            mkf( _VM_F_Memory ) << "attempted to dereference a broken pointer " << p << dsc;
            return false;
        }

        if ( pp.heap() )
        {
            HeapPointer hp = pp;
            if ( hp.null() || !heap().valid( hp ) )
            {
                mkf( _VM_F_Memory ) << "invalid pointer dereference " << p << dsc;
                return false;
            }
            width = heap().size( hp );
        }
        else if ( pp.type() == PointerType::Global )
        {
            if ( write && ptr2s( pp ).location == Slot::Const )
            {
                mkf( _VM_F_Memory ) << "attempted write to a constant location " << p << dsc;
                return false;
            }

            if ( pp.object() >= program()._globals.size() )
            {
                mkf( _VM_F_Memory ) << "pointer object out of bounds in " << p << dsc;
                return false;
            }

            width = ptr2s( pp ).size();
        }

        if ( int64_t( pp.offset() ) + sz > width )
        {
            mkf( _VM_F_Memory ) << "access of size " << sz << " at " << p
                                << " is " << pp.offset() + sz - width
                                << " bytes out of bounds";
            return false;
        }
        return true;
    }

}
