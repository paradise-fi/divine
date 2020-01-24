// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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
#include <divine/vm/pointer.hpp>
#include <divine/vm/divm.h>

namespace divine::vm::lx
{

template< typename Heap >
struct Types
{
    Heap &_heap;
    HeapPointer _base;

    _VM_TypeTable type( int idx )
    {
        auto ptr = _base + idx * sizeof( _VM_TypeTable );
        return *_heap.template unsafe_deref< _VM_TypeTable >( ptr, _heap.ptr2i( ptr ) );
    }

    std::pair< int64_t, int > subtype( int id, int64_t sub )
    {
        switch ( type( id ).type.type )
        {
            case _VM_Type::Array:
            {
                auto tid = type( id + 1 ).item.type_id;
                return std::make_pair( sub * allocsize( tid ), tid );
            }
            case _VM_Type::Struct:
            {
                ASSERT_LT( sub, type( id ).type.items );
                auto it = type( id + 1 + sub ).item;
                return std::make_pair( int64_t( it.offset ), it.type_id );
            }
            default:
                UNREACHABLE( "attempted to obtain an offset into a scalar, type =", id );
        }
    }

    int64_t allocsize( int id ) { return type( id ).type.size; }
    Types( Heap &h, HeapPointer b ) : _heap( h ), _base( b ) {}
};

}
