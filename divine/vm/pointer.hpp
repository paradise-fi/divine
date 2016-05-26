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

enum class PointerType : unsigned { Code, Const, Global, Heap };

static const int PointerBits = 64;
static const int PointerBytes = PointerBits / 8;
static const int PointerObjBits = 32;
static const int PointerTypeBits = 2;
static const int PointerOffBits = PointerBits - (PointerTypeBits + PointerObjBits);
using PointerRaw = brick::mem::bitvec< PointerBits >;

/*
 * There are multiple pointer types, distinguished by a two-bit type tag. The
 * generic pointer cannot be used directly, but it is the same size as all
 * other pointer types and it retains the type-specific content (i.e. it can be
 * treated like a value). All non-code pointers are ultimately resolved through
 * the heap, but for Const and Global pointers through an indirection.
 */

struct GenericPointer : brick::types::Comparable
{
    static const int ObjBits = 32;
    static const int TypeBits = 2;
    static const int OffBits = PointerBits - ObjBits - TypeBits;

    using ObjT = brick::mem::bitvec< PointerObjBits >;
    using OffT = brick::mem::bitvec< PointerOffBits >;

    union Rep { /* FIXME type punning */
        PointerRaw raw;
        struct {
            ObjT obj;
            PointerType type:PointerTypeBits;
            OffT off:PointerOffBits;
        };
    } _rep;

    static_assert( sizeof( Rep ) == PointerBytes );

    explicit GenericPointer( PointerType t, ObjT obj = 0, OffT off = 0 )
    {
        _rep.obj = obj;
        _rep.off = off;
        _rep.type = t;
    }

    ObjT object() { return _rep.obj; }
    OffT offset() { return _rep.off; }
    void offset( OffT o ) { _rep.off = o; }
    void object( ObjT o ) { _rep.obj = o; }
    PointerRaw raw() { return _rep.raw; }
    void raw( PointerRaw r ) { _rep.raw = r; }

    bool operator<= ( GenericPointer o ) const
    {
        return _rep.raw <= o._rep.raw;
    }

    auto type() { return _rep.type; }
    void type( PointerType t ) { _rep.type = t; }

    template< typename P,
              typename Q = typename std::enable_if<
                  std::is_same< Rep, typename P::Rep >::value >::type >
    operator P() { P rv; rv._rep = _rep; return rv; }
};

static inline GenericPointer nullPointer() { return GenericPointer( PointerType::Code ); }

/*
 * Points to a particular instruction in the program. Constant code pointers
 * can only point at functions or labels, not arbitrary instructions; however,
 * the unit of the pointer is an instruction (not a byte), and incrementing a
 * code pointer will give you a pointer to the next instruction. Program
 * counters are implemented as code pointers.
 */
struct CodePointer : GenericPointer
{
    CodePointer( ObjT f = 0, OffT i = 0 ) : GenericPointer( PointerType::Code, f, i ) {}

    auto function() { return _rep.obj; }
    void function( ObjT f ) { _rep.obj = f; }
    auto instruction() { return _rep.off; }
    void instruction( OffT i ) { _rep.off = i; }
};

/*
 * Pointer to constant memory. This is separate from a GlobalPointer, because
 * it allows an important optimisation of memory footprint in PersistentHeap.
 */
struct ConstPointer : GenericPointer
{
    ConstPointer( ObjT obj = 0, OffT off = 0 ) : GenericPointer( PointerType::Const, obj, off ) {}
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
struct GlobalPointer : GenericPointer
{
    GlobalPointer( ObjT obj = 0, OffT off = 0 ) : GenericPointer( PointerType::Global, obj, off ) {}
};

/* HeapPointer does not exist here, its layout is defined by a Memory object */

static inline std::ostream &operator<<( std::ostream &o, GenericPointer p )
{
    return o << "[generic " << int( p.type() ) << " " << p.object() << " " << p.offset() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, CodePointer p )
{
    return o << "[code " << p.function() << " " << p.instruction() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, GlobalPointer p )
{
    return o << "[global " << p.object() << " " << p.offset() << "]";
}

static inline std::ostream &operator<<( std::ostream &o, ConstPointer p )
{
    return o << "[const " << p.object() << " " << p.offset() << "]";
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

template<>
struct hash< divine::vm::GenericPointer > {
    size_t operator()( divine::vm::GenericPointer ptr ) const
    {
        return *static_cast< uint32_t * >( ptr._v.storage );
    }
};

}
