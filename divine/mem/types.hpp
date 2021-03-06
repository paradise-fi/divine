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

#include <divine/vm/divm.h>

#include <deque>
#include <tuple>

namespace divine::mem
{

    template< int _slab_bits = 20 >
    struct PoolRep
    {
        static const int slab_bits = _slab_bits, chunk_bits = 16, tag_bits = 28;
        uint64_t slab:slab_bits, chunk:chunk_bits, tag:tag_bits;
    };

    template< typename Next > struct Frontend;
    template< typename Next > struct Data;
    template< typename Next > struct Cow;
    template< typename Next > struct UserMeta;

    template< typename Next > struct Metadata;
    template< typename Next > struct TaintLayer;
    template< typename Next > struct DefinednessLayer;
    template< typename Next > struct PointerLayer;
    template< typename Next > struct ShadowBase;
    template< typename Next > struct CompressPDT;

    template< typename HeapPtr, typename PointerV, template< int, bool > class IntV, typename Pool >
    struct Base;

    /* ShadowLayers are configurable to store only required metadata to a
     * shadow memory attached to a heap pointer.
     *
     * Each kind of metadata has its own layer. The goal of each layer is to
     * transform information from expanded form metadata to internal metadata
     * representation in vm::value. Additionally, layers store metadata
     * exceptions related maps and manage their manipulations (copying).
     *
     * All layers have unified interface (see ShadowBase) to enable chaining of
     * operations.
     *
     * It is required by shadow storage to provide interface layer for upper
     * layers (Metadata). DiVM requires presence of the PointerLayer to be
     * able to reconstruct fragmented pointers. Intermediate layers (Taint,
     * Definedness, Pointer) implement various metadata chunks. ShadowBase is
     * a trivial metadata storage that defines interface to shadow memory.  At
     * the bottom of configuration layers is a compression layer. It is used to
     * compress and decompress metadata from actual shadow memory.
     */
    template< typename B >
    using ShadowLayers = Metadata<
                         TaintLayer<
                         DefinednessLayer<
                         PointerLayer<
                         ShadowBase<
                         CompressPDT< B > > > > > >;

    template< int slab >
    using Pool = brick::mem::Pool< PoolRep< slab > >;

    template< typename B >
    using HeapBase = Data< UserMeta< ShadowLayers< B > > >;

    template< typename B >
    using MutableHeap = Frontend< HeapBase< B > >;

}
