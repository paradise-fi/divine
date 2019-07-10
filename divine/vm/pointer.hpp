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
                                    Alloca = _VM_PT_Alloca,
                                    Heap = _VM_PT_Heap,
                                    Weak = _VM_PT_Weak,
                                    Marked = _VM_PT_Marked,
                                  };

static const int PointerBytes = _VM_PB_Full / 8;
using PointerRaw = bitlevel::bitvec< _VM_PB_Full >;

/* There are multiple pointer types, distinguished by the range of object id's
 * they fall into. The generic pointer cannot be used directly, but it is the
 * same size as all other pointer types and it retains the type-specific
 * content (i.e. it can be treated like a value). All data pointers are
 * ultimately resolved through the heap, but Global pointers go through an
 * indirection. */

struct GenericPointer : brick::types::Comparable
{
    static const int ObjBits  = _VM_PB_Obj;
    static const int OffBits  = _VM_PB_Off;

    using ObjT = bitlevel::bitvec< ObjBits >;
    using OffT = bitlevel::bitvec< OffBits >;
    using Type = PointerType;

    PointerRaw _raw;

    struct Rep /* FIXME little endian */
    {
        Rep( ObjT obj = 0, OffT off = 0 ) : off( off ), obj( obj ) {}
        OffT off;
        ObjT obj;
    };

    static_assert( sizeof( Rep ) == PointerBytes );

    explicit GenericPointer( ObjT obj, OffT off = 0 )
    {
        brq::bitcast( Rep( obj, off ), _raw );
    }

    explicit GenericPointer( Rep r = Rep() ) { brq::bitcast( r, _raw ); }

    Rep rep() const { return brq::bitcast< Rep >( _raw ); }
    ObjT object() const { return rep().obj; }
    OffT offset() const { return rep().off; }
    void offset( OffT o ) { auto r = rep(); r.off = o; brq::bitcast( r, _raw ); }
    void object( ObjT o ) { auto r = rep(); r.obj = o; brq::bitcast( r, _raw ); }
    PointerRaw raw() const { return _raw; }
    void raw( PointerRaw r ) { _raw = r; }
    bool null() const { return object() == 0; } /* check whether a pointer is null */

    bool operator< ( GenericPointer o ) const
    {
        return raw() < o.raw();
    }

    Type type() const
    {
        return Type( __vm_pointer_type( object() ) );
    }

    bool heap() const { return object() >= _VM_PL_Code; }
    GenericPointer operator+( int o ) { return GenericPointer( object(), offset() + o ); }
};

/* the canonic null pointer, do *not* use as a null check through comparison;
 * see GenericPointer::null() instead */
static inline GenericPointer nullPointer()
{
    return GenericPointer( 0, 0 );
}

/* Points to a particular instruction in the program. Constant code pointers
 * can only point at functions or labels, not arbitrary instructions; however,
 * the unit of the pointer is an instruction (not a byte), and incrementing a
 * code pointer will give you a pointer to the next instruction. Program
 * counters are implemented as code pointers. */

struct CodePointer : GenericPointer
{
    explicit CodePointer () // Canonic null
        : GenericPointer( 0, 0 ) {}
    explicit CodePointer( ObjT f, OffT i = 0 )
        : GenericPointer( f | _VM_PL_Global , i )
    {
        ASSERT_LT( object(), _VM_PL_Code );
    }

    CodePointer( GenericPointer r )
        : GenericPointer( r )
    {
        if ( r.object() )
            ASSERT_EQ( type(), PointerType::Code );
    }

    auto function() const
    {
        return object() & ~_VM_PL_Global;
    }
    void function( ObjT f )
    {
        ASSERT_LT( f, _VM_PL_Global );
        object( f | _VM_PL_Global );
    }
    auto instruction() const { return offset(); }
    void instruction( OffT i ) { offset( i ); }
    bool null() const { return function() == 0; }
};

template< PointerType T >
struct SlotPointer : GenericPointer
{
    explicit SlotPointer( ObjT obj = 0, OffT off = 0 )
        : GenericPointer( obj, off ) {}

    SlotPointer( GenericPointer r ) : GenericPointer( r )
    {
        if ( ! null() )
            ASSERT_EQ( type(), T );
    }
};

/* Pointer to a global variable. Global pointers need to be indirected, because
 * multiple copies of the program (processes) must be able to co-exist within a
 * single heap. A special per-process heap object is created to hold global
 * variables (similar to a frame). Its layout is dictated by the Program
 * object, the 'obj' part of the global pointer determines which Slot within
 * the global variable object this pointer points to. The offset is in bytes,
 * counting from the start of the slot. */
using GlobalPointer = SlotPointer< PointerType::Global >;

struct HeapPointer : GenericPointer
{
    HeapPointer( ObjT obj = 0, OffT off = 0 ) : GenericPointer( obj, off ) {}
    HeapPointer( GenericPointer r ) : GenericPointer( r )
    {
        if ( ! null() )
            ASSERT( heap() );
    }
};


template< typename stream >
auto operator<<( stream &o, PointerType p ) -> decltype( o << "" )
{
    switch ( p )
    {
        case PointerType::Global: return o << "global";
        case PointerType::Code: return o << "code";
        case PointerType::Alloca: return o << "alloca";
        case PointerType::Heap: return o << "heap";
        case PointerType::Weak: return o << "weak";
        case PointerType::Marked: return o << "marked";
    }
    return o << "ptr" << int( p );
}

template< typename stream, typename P,
          typename Q = typename std::enable_if<
              std::is_same< decltype( P().type() ), PointerType >::value >::type >
static inline auto operator<<( stream &o, P p ) -> decltype( o << "" )
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
