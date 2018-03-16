// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/heap.hpp>
#include <divine/vm/heap.tpp>

namespace divine::vm
{

using Int64 = value::Int< 64, false >;
using Int32 = value::Int< 32, false >;
using Int16 = value::Int< 16, false >;
using Int8 = value::Int< 8, false >;
using Int1 = value::Int< 1, false >;
using Float80 = value::Float< long double >;
using Float64 = value::Float< double >;
using Float32 = value::Float< float >;
using Pointer = value::Pointer;

template < int i > using PoolPtr = brick::mem::PoolPointer< PoolRep< i > >;

template struct SimpleHeap< CowHeap >;
template struct SimpleHeap< SmallHeap, PoolRep< 8 > >;
template struct SimpleHeap< MutableHeap >;

template void SimpleHeap< CowHeap >::read<Int64>( HeapPointer, Int64&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Int32>( HeapPointer, Int32&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Int16>( HeapPointer, Int16&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Int8>( HeapPointer, Int8&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Int1>( HeapPointer, Int1&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Float80>( HeapPointer, Float80&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Float64>( HeapPointer, Float64&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Float32>( HeapPointer, Float32&, PoolPtr<20> ) const;
template void SimpleHeap< CowHeap >::read<Pointer>( HeapPointer, Pointer&, PoolPtr<20> ) const;

template void SimpleHeap< SmallHeap, PoolRep<8> >::read<Int8>( HeapPointer, Int8&, PoolPtr<8> ) const;

}
