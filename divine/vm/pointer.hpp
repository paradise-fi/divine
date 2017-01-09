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

#include <runtime/divine.h>

namespace divine {
namespace vm {

namespace bitlevel = brick::bitlevel;

enum class PointerType : unsigned { Const, Global, Heap, Code, Weak, Marked };

static const int PointerBytes = _VM_PB_Full / 8;
using PointerRaw = bitlevel::bitvec< _VM_PB_Full >;

/*
 * There are multiple pointer types, distinguished by a two-bit type tag. The
 * generic pointer cannot be used directly, but it is the same size as all
 * other pointer types and it retains the type-specific content (i.e. it can be
 * treated like a value). All non-code pointers are ultimately resolved through
 * the heap, but for Const and Global pointers through an indirection.
 */

struct GenericPointer : brick::types::Comparable
{
    static const int ObjBits  = _VM_PB_Obj;
    static const int TypeBits = _VM_PB_Type;
    static const int OffBits  = _VM_PB_Off;

    using ObjT = bitlevel::bitvec< ObjBits >;
    using OffT = bitlevel::bitvec< OffBits >;

    union Rep { /* note: type punning is OK in clang */
        PointerRaw raw;
        struct { // beware: bitfields seem to be little-endian in clang but it is impmentation defined
            OffT off:OffBits; // offset must be last (for the sake of arithmetic)
            PointerType type:TypeBits;
            ObjT obj;
        };
    } _rep;

    static_assert( sizeof( Rep ) == PointerBytes );

    explicit GenericPointer( PointerType t, ObjT obj = 0, OffT off = 0 )
    {
        _rep.obj = obj;
        _rep.off = off;
        _rep.type = t;
    }

    explicit GenericPointer( Rep r = Rep() ) : _rep( r ) {}

    ObjT object() { return _rep.obj; }
    OffT offset() { return _rep.off; }
    void offset( OffT o ) { _rep.off = o; }
    void object( ObjT o ) { _rep.obj = o; }
    PointerRaw raw() { return _rep.raw; }
    void raw( PointerRaw r ) { _rep.raw = r; }
    bool null() { return object() == 0; } /* check whether a pointer is null */

    bool operator<= ( GenericPointer o ) const
    {
        return _rep.raw <= o._rep.raw;
    }

    auto type() { return _rep.type; }
    void type( PointerType t ) { _rep.type = t; }
    bool heap() { return type() == PointerType::Heap ||
                         type() == PointerType::Weak ||
                         type() == PointerType::Marked; }

    GenericPointer operator+( int o ) { return GenericPointer( type(), object(), offset() + o ); }
};

/* the canonic null pointer, do *not* use as a null check through comparison;
 * see GenericPointer::null() instead */
static inline GenericPointer nullPointer( PointerType t = PointerType::Const )
{
    return GenericPointer( t );
}

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
    CodePointer( GenericPointer r ) : GenericPointer( r ) { ASSERT_EQ( type(), PointerType::Code ); }

    auto function() const { return _rep.obj; }
    void function( ObjT f ) { _rep.obj = f; }
    auto instruction() const { return _rep.off; }
    void instruction( OffT i ) { _rep.off = i; }
};

/*
 * Pointer to constant memory. This is separate from a GlobalPointer, because
 * a) it allows an important optimisation of memory footprint b) improves
 * clarity and error checking.
 */
struct ConstPointer : GenericPointer
{
    ConstPointer( ObjT obj = 0, OffT off = 0 )
        : GenericPointer( PointerType::Const, obj, off ) {}
    ConstPointer( GenericPointer r ) : GenericPointer( r )
    {
        if ( null() )
            type( PointerType::Const );
        ASSERT_EQ( type(), PointerType::Const );
    }
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
    explicit GlobalPointer( ObjT obj = 0, OffT off = 0 )
        : GenericPointer( PointerType::Global, obj, off ) {}
    GlobalPointer( GenericPointer r ) : GenericPointer( r )
    {
        if ( null() )
            type( PointerType::Global );
        ASSERT_EQ( type(), PointerType::Global );
    }
};

struct HeapPointer : GenericPointer
{
    HeapPointer( ObjT obj = 0, OffT off = 0 ) : GenericPointer( PointerType::Heap, obj, off ) {}
    HeapPointer( GenericPointer r ) : GenericPointer( r )
    {
        if ( null() )
            type( PointerType::Heap );
        ASSERT( heap() );
    }
};


static inline std::ostream &operator<<( std::ostream &o, PointerType p )
{
    switch ( p )
    {
        case PointerType::Const: return o << "const";
        case PointerType::Global: return o << "global";
        case PointerType::Code: return o << "code";
        case PointerType::Heap: return o << "heap";
        case PointerType::Weak: return o << "weak";
        case PointerType::Marked: return o << "marked";
    }
}

template< typename P,
          typename Q = typename std::enable_if<
              std::is_same< decltype( P().type() ), PointerType >::value >::type >
static inline std::ostream &operator<<( std::ostream &o, P p )
{
    return o << p.type() << "* " << std::hex << p.object() << " " << p.offset();
}

}

namespace t_vm {

struct TestPointer
{
    TEST(conversion)
    {
        vm::ConstPointer c( 1, 3 );
        ASSERT_EQ( c, vm::ConstPointer( vm::GenericPointer( c ) ) );
    }
};

}

}

namespace std {

template<>
struct hash< divine::vm::GenericPointer > {
    size_t operator()( divine::vm::GenericPointer ptr ) const
    {
        return ptr.raw();
    }
};

template<>
struct hash< divine::vm::HeapPointer > {
    size_t operator()( divine::vm::GenericPointer ptr ) const
    {
        return ptr.raw();
    }
};

}
