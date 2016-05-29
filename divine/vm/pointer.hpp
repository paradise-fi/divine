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

#include <brick-bitlevel>
#include <brick-mem>

namespace divine {
namespace vm {

namespace bitlevel = brick::bitlevel;

enum class PointerType { Code, Const, Global, Heap };

static const int PointerBits = 64;
static const int PointerBytes = PointerBits / 8;
static const int PointerTypeBits = 2;
static const int PointerObjBits = 20;
static const int PointerOffBits = PointerBits - (PointerTypeBits + PointerObjBits);
using PointerRaw = brick::mem::bitvec< PointerBits >;
template< int i > using PointerField = bitlevel::BitField< PointerRaw, i >;

/*
 * There are multiple pointer types, distinguished by a two-bit type tag. The
 * generic pointer cannot be used directly, but it is the same size as all
 * other pointer types and it retains the type-specific content (i.e. it can be
 * treated like a value). All non-code pointers are ultimately resolved through
 * the heap, but for Const and Global pointers through an indirection.
 */

template< typename F1 = PointerField< PointerBits - PointerTypeBits >, typename... Fs >
struct GenericPointer : brick::types::Comparable
{
    using Generic = GenericPointer<>;
    using Bits = bitlevel::BitTuple<
        bitlevel::BitField< PointerType, PointerTypeBits >, F1, Fs... >;
    Bits _v;
    static_assert( sizeof( _v ) * 8 == PointerBits, "pointer size mismatch" );

    GenericPointer( PointerType t )
    {
        bitlevel::get< 0 >( _v ).set( t );
    }

    bool operator<= ( GenericPointer o ) const
    {
        return _v < o._v || _v == o._v;
    }

    template< int i >
    auto field() { return bitlevel::get< i >( _v ); }
    auto type() { return field< 0 >(); }

    template< typename P >
    P convertTo( P x = P() )
    {
        std::copy( _v.storage, _v.storage + sizeof( _v.storage ), x._v.storage );
        return x;
    }

    template< typename P,
              typename Q = typename std::enable_if<
                  sizeof( _v.storage ) == sizeof( P::Bits::storage ) >::type,
              typename R = decltype( P() ) >
    operator P() { return convertTo< P >(); }
    operator Generic() { return convertTo< Generic >( Generic( PointerType::Code ) ); }
};

static inline GenericPointer<> nullPointer() { return GenericPointer<>( PointerType::Code ); }

template< int obj_w, int off_w >
using SplitPointerBase = GenericPointer< PointerField< obj_w >, PointerField< off_w > >;

template< int obj_w = PointerObjBits, int off_w = PointerOffBits >
struct SplitPointer : SplitPointerBase< obj_w, off_w >
{
    SplitPointer( PointerType t, int obj = 0, int offset = 0 )
        : SplitPointerBase< obj_w, off_w >( t )
    {
        this->template field< 1 >().set( obj );
        this->template field< 2 >().set( offset );
    }

    auto object() { return this->template field< 1 >(); }
    auto offset() { return this->template field< 2 >(); }
};

/*
 * Points to a particular instruction in the program. Constant code pointers
 * can only point at functions or labels, not arbitrary instructions; however,
 * the unit of the pointer is an instruction (not a byte), and incrementing a
 * code pointer will give you a pointer to the next instruction. Program
 * counters are implemented as code pointers.
 */
struct CodePointer : SplitPointer<>
{
    CodePointer( int f = 0, int i = 0 ) : SplitPointer( PointerType::Code, f, i ) {}

    auto function() { return this->template field< 1 >(); }
    auto instruction() { return this->template field< 2 >(); }
};

/*
 * Pointer to constant memory. This is separate from a GlobalPointer, because
 * it allows an important optimisation of memory footprint in PersistentHeap.
 */
struct ConstPointer : SplitPointer<>
{
    ConstPointer( int obj = 0, int off = 0 ) : SplitPointer( PointerType::Const, obj, off ) {}
};

/*
 * Pointer to a global variable. Global pointers need to be indirected, because
 * multiple copies of the program (processes) must be able to co-exist within a
 * single heap. A special per-process heap object is created to hold global
 * variables (similar to a frame). Its layout is dictated by the Program
 * object, the 'obj' part of the global pointer determines which Slot within
 * the global variable object this pointer points to. The offset is in bytes,
 * counting from the start of the slot.
 */
struct GlobalPointer : SplitPointer<>
{
    GlobalPointer( int obj = 0, int off = 0 ) : SplitPointer( PointerType::Global, obj, off ) {}
};

/* HeapPointer does not exist here, its layout is defined by a Memory object */

static inline std::ostream &operator<<( std::ostream &o, GenericPointer p )
{
    return o << "[generic " << int( p.type() ) << " " << p.object() << " " << p.offset() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, CodePointer p )
{
    return o << "[code " << p.function().get() << " " << p.instruction().get() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, GlobalPointer p )
{
    return o << "[global " << p.object().get() << " " << p.offset().get() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, ConstPointer p )
{
    return o << "[const " << p.object().get() << " " << p.offset().get() << "]";
}

}

namespace t_vm {

struct TestPointer
{
    TEST(conversion)
    {
        vm::ConstPointer c( 1, 3 );
        ASSERT_EQ( c, vm::ConstPointer( vm::CodePointer( c ) ) );
    }
};

}

}

namespace std {

template< typename... Bits >
struct hash< divine::vm::GenericPointer< Bits... > > {
    size_t operator()( divine::vm::GenericPointer< Bits... > ptr ) const
    {
        return *reinterpret_cast< uint32_t * >( ptr._v.storage );
    }
};

}
