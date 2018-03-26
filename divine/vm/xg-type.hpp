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
#include <divine/vm/mem-heap.hpp>
#include <divine/vm/lx-type.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/DataLayout.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::vm::xg
{

struct Types
{
    std::map< const llvm::Type *, int > _ids;
    std::vector< _VM_TypeTable > _types;
    llvm::DataLayout TD;

    int add( llvm::Type *T )
    {
        if ( _ids.count( T ) )
            return _ids[ T ];

        int key = _ids[ T ] = _types.size();
        _types.emplace_back();
        _VM_Type &t = _types.back().type;
        t.size = T->isSized() ? TD.getTypeAllocSize( T ) : 0;
        int items = 0;

        if ( T->isStructTy() )
        {
            t.type = _VM_Type::Struct;
            items = t.items = T->getStructNumElements();
            _types.resize( _types.size() + items );
        }
        else if ( auto ST = llvm::dyn_cast< llvm::SequentialType >( T ) )
        {
            t.type = _VM_Type::Array;
            t.items = T->isArrayTy() ? T->getArrayNumElements() : 0;
            int ar_id = _types.size();
            _types.emplace_back();
            int tid = add( ST->getElementType() );
            _types[ ar_id ].item.offset = 0;
            _types[ ar_id ].item.type_id = tid;
        }
        else
            t.type = _VM_Type::Scalar;

        auto ST = llvm::dyn_cast< llvm::StructType >( T );
        if ( ST && !ST->isOpaque() )
        {
            auto SL = TD.getStructLayout( ST );
            for ( int i = 0; i < items; ++i )
            {
                _VM_TypeItem item { .offset = unsigned( SL->getElementOffset( i ) ),
                                    .type_id = add( ST->getElementType( i ) ) };
                _types[ key + 1 + i ].item = item;
            }
        }

        return key;
    }

    template< typename Heap >
    HeapPointer emit( Heap &h )
    {
        int sz = _types.size() * sizeof( _VM_TypeTable );
        auto rv = h.make( sz );
        auto w = rv.cooked();
        CharV nil( 0 );
        for ( int i = 0; i < sz; ++i )
            h.write( w + i, nil ); /* make the memory defined */
        for ( auto t : _types )
        {
            *h.template unsafe_deref< _VM_TypeTable >( w, h.ptr2i( w ) ) = t;
            w = w + sizeof( _VM_TypeTable );
        }
        return rv.cooked();
    }

    Types( llvm::Module *m ) : TD( m ) {}
};

}
