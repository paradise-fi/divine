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
#include <divine/vm/value.hpp>
#include <divine/vm/divm.h>

namespace divine::vm::lx
{

/*
 * Values (data) used in a program are organised into blocks of memory:
 * frames (one for each function), globals (for each process) and constants
 * (immutable and shared across all processes). These blocks consist of
 * individual slots, one slot per value stored in the given block. Slots
 * are typed and they can overlap *if* the lifetimes of the values stored
 * in them do not. In this respect, slots behave like pointers into the
 * given memory block. All slots are assigned statically. All LLVM Values
 * are allocated into slots.
 */
struct Slot : _VM_Operand, _VM_OperandExt
{
    bool pointer() { return type == Ptr || type == PtrA; }
    bool alloca() { return type == PtrA; }
    bool integer() { return type == I1 || type == I8 || type == I16 || type == I32 || type == I64 ||
                            type == I128 || type == IX; }
    bool isfloat() { return type == F32 || type == F64 || type == F80; }
    bool aggregate() { return type == Agg; }
    bool codePointer() { return type == PtrC; }
    bool ext() { return type == Other || type == Agg; }

    int size() const
    {
        if ( type == F80 ) return 16; /* f80 is magical */
        return width() % 8 ? width() / 8 + 1 : width() / 8;
    }

    int width() const
    {
        switch ( type )
        {
            case Void: return 0;
            case I1: return 1;
            case I8: return 8;
            case I16: return 16;
            case I32: return 32;
            case I64: return 64;
            case I128: return 128;
            case F32: return 32;
            case F64: return 64;
            case F80: return 80;
            case Ptr: case PtrA: case PtrC: return 64;
            default: return _VM_OperandExt::width;
        }
    }

    explicit Slot( Location l = Invalid, Type t = Void, int w = 0 )
    {
        type = t;
        location = l;
        _VM_OperandExt::width = w;
        if ( w )
            ASSERT_EQ( w, width() );
        if ( w == 1 || w == 8 || w == 16 || w == 32 || w == 64 )
            ASSERT_NEQ( type, IX );
    }

    explicit Slot( _VM_Operand op, _VM_OperandExt ext = _VM_OperandExt() )
        : _VM_Operand( op ), _VM_OperandExt( ext )
    {}

    Slot &operator=( const Slot & ) & = default;

    friend std::ostream &operator<<( std::ostream &o, Slot p )
    {
        static std::vector< std::string > t =
            { "i1", "i8", "i16", "i32", "i64", "f32", "f64", "f80",
              "ptr", "ptrc", "ptra", "agg", "void", "other" };
        static std::vector< std::string > l = { "const", "global", "local", "invalid" };
        return o << "[" << l[ p.location ] << " " << t[ p.type ] << " @" << p.offset << " ↔"
                 << p.width() << "]";
    }
};

}
