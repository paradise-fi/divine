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
#include <divine/vm/pointer.hpp>
#include <divine/mem/types.hpp>

#include <deque>
#include <tuple>

namespace divine::vm
{

    namespace value
    {
        struct Pointer;
        template< int, bool, bool > struct GenInt;
        template< int w, bool s = false, bool dyn = false > struct Int;
        template< int w, bool s > using FixInt = Int< w, s, false >;
        template< bool s = false > struct DynInt;
    }

    struct Interrupt : brick::types::Ord
    {
        enum Type { Mem, Cfl } type:1;
        uint32_t ictr:31;
        CodePointer pc;
        auto as_tuple() const { return std::make_tuple( type, ictr, pc ); }
        Interrupt( Type t = Mem, uint32_t ictr = 0, CodePointer pc = CodePointer() )
            : type( t ), ictr( ictr ), pc( pc )
        {}
    };

    template< typename stream >
    static inline auto operator<<( stream &o, Interrupt i ) -> decltype( o << "" )
    {
        return o << ( i.type == Interrupt::Mem ? 'M' : 'C' ) << "/" << i.ictr
                 << "/" << i.pc.function() << ":" << i.pc.instruction();
    }

    struct Choice : brick::types::Ord
    {
        int taken, total;
        Choice( int taken, int total ) : taken( taken ), total( total ) {}
        auto as_tuple() const { return std::make_tuple( taken, total ); }
    };

    struct ChoiceOptions
    {
        int options;
        // might be empty or contain probability for each option
        std::vector< int > p;
    };

    struct Program;

    template< typename stream >
    auto operator<<( stream &o, Choice i ) -> decltype( o << "" )
    {
        return o << i.taken << "/" << i.total;
    }

    struct Step
    {
        std::deque< Interrupt > interrupts;
        std::deque< Choice > choices;
    };

    using Fault = ::_VM_Fault;

    struct TraceText { GenericPointer text; };
    struct TraceFault { std::string string; };
    struct TraceSchedInfo { int pid; int tid; };
    struct TraceTaskID { GenericPointer ptr; };
    struct TraceStateType { CodePointer pc; };
    struct TraceInfo { GenericPointer text; };
    struct TraceAssume { HeapPointer ptr; };
    struct TraceConstraints { HeapPointer ptr; };
    struct TraceLeakCheck {};
    struct TraceTypeAlias { CodePointer pc; GenericPointer alias; };
    struct TraceDebugPersist { GenericPointer ptr; };

    template< typename Context > struct FaultStream;
    template< typename _Program, typename _Heap > struct Context;
    template< typename Context > struct Eval;

    template< int slab >
    using HeapBase = mem::Base< HeapPointer, value::Pointer, value::FixInt, mem::Pool< slab > >;

    struct MutableHeap;
    struct SmallHeap;
    struct CowHeap;

    using CowSnapshot = brick::mem::Pool< mem::PoolRep<> >::Pointer;

}
