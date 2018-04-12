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

#define __divm_const__ /* we need the constants but not anything else here */
#include <divine/vm/divm.h>
#undef __divm_const__

namespace divine {
namespace vm {

namespace bitlevel = brick::bitlevel;

enum class PointerType : unsigned { Global = _VM_PT_Global,
                                    Code = _VM_PT_Code,
                                    Heap = _VM_PT_Heap,
                                    Weak = _VM_PT_Weak,
                                    Marked = _VM_PT_Marked,
                                    Local = _VM_PT_Local };

static const int PointerBytes = _VM_PB_Full / 8;
using PointerRaw = bitlevel::bitvec< _VM_PB_Full >;

/*
 * There are multiple pointer types, distinguished by a three-bit type tag. The
 * generic pointer cannot be used directly, but it is the same size as all
 * other pointer types and it retains the type-specific content (i.e. it can be
 * treated like a value). All pointers are ultimately resolved through the
 * heap, but Global pointers go through an indirection.
 */

struct GenericPointer : brick::types::Comparable
{
    static const int ObjBits  = _VM_PB_Obj;
    static const int TypeBits = _VM_PB_Type;
    static const int OffBits  = _VM_PB_Off;

    using ObjT = bitlevel::bitvec< ObjBits >;
    using OffT = bitlevel::bitvec< OffBits >;
    using Type = PointerType;

    union Rep { /* note: type punning is OK in clang */
        PointerRaw raw;
        struct { // beware: bitfields seem to be little-endian in clang but it is impmentation defined
            OffT off:OffBits; // offset must be last (for the sake of arithmetic)
            PointerType type:TypeBits;
            ObjT obj;
        };
    } _rep;

    static_assert( sizeof( Rep ) == PointerBytes );

    explicit GenericPointer( Type t, ObjT obj = 0, OffT off = 0 )
    {
        _rep.obj = obj;
        _rep.off = off;
        _rep.type = t;
    }

    explicit GenericPointer( Rep r = Rep() ) : _rep( r ) {}

    ObjT object() const { return _rep.obj; }
    OffT offset() const { return _rep.off; }
    void offset( OffT o ) { _rep.off = o; }
    void object( ObjT o ) { _rep.obj = o; }
    PointerRaw raw() const { return _rep.raw; }
    void raw( PointerRaw r ) { _rep.raw = r; }
    bool null() const { return object() == 0; } /* check whether a pointer is null */

    bool operator< ( GenericPointer o ) const
    {
        return _rep.raw < o._rep.raw;
    }

    auto type() { return _rep.type; }
    void type( Type t ) { _rep.type = t; }
    bool heap() { return type() == Type::Heap ||
                         type() == Type::Weak ||
                         type() == Type::Marked; }

    GenericPointer operator+( int o ) { return GenericPointer( type(), object(), offset() + o ); }
};

/* the canonic null pointer, do *not* use as a null check through comparison;
 * see GenericPointer::null() instead */
static inline GenericPointer nullPointer( PointerType t = PointerType::Global )
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
    explicit CodePointer( ObjT f = 0, OffT i = 0 ) : GenericPointer( PointerType::Code, f, i ) {}
    CodePointer( GenericPointer r ) : GenericPointer( r ) { ASSERT_EQ( type(), PointerType::Code ); }

    auto function() const { return _rep.obj; }
    void function( ObjT f ) { _rep.obj = f; }
    auto instruction() const { return _rep.off; }
    void instruction( OffT i ) { _rep.off = i; }
};

template< PointerType T >
struct SlotPointer : GenericPointer
{
    explicit SlotPointer( ObjT obj = 0, OffT off = 0 )
        : GenericPointer( T, obj, off ) {}

    SlotPointer( GenericPointer r ) : GenericPointer( r )
    {
        if ( null() )
            type( T );
        ASSERT_EQ( type(), T );
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
using GlobalPointer = SlotPointer< PointerType::Global >;
using LocalPointer = SlotPointer< PointerType::Local >;

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
        case PointerType::Global: return o << "global";
        case PointerType::Code: return o << "code";
        case PointerType::Heap: return o << "heap";
        case PointerType::Weak: return o << "weak";
        case PointerType::Marked: return o << "marked";
        case PointerType::Local: return o << "local";
    }
    return o << "ptr" << int( p );
}

template< typename P,
          typename Q = typename std::enable_if<
              std::is_same< decltype( P().type() ), PointerType >::value >::type >
static inline std::ostream &operator<<( std::ostream &o, P p )
{
    o << p.type() << "* " << std::hex << p.object() << " " << p.offset();
    if ( p.offset() >= 16 && p.offset() % 16 < 10 ) o << "h";
    return o << std::dec;
}

}

namespace t_vm {

struct TestPointer
{
    TEST(conversion)
    {
        vm::GlobalPointer g( 1, 3 );
        ASSERT_EQ( g, vm::GlobalPointer( vm::GenericPointer( g ) ) );
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
