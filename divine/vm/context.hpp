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

#include <divine/vm/value.hpp>
#include <divine/vm/program.hpp>
#include <divine/vm/setup.hpp>

#include <unordered_set>

namespace divine {
namespace vm {

using Fault = ::_VM_Fault;

struct TraceText { value::Pointer text; };
struct TraceFlag { int flag; };
struct TraceSchedChoice { value::Pointer list; };
struct TraceSchedInfo { int pid; int tid; };

template< typename _Heap >
struct Context
{
    using Heap = _Heap;
    using PointerV = value::Pointer;

    struct Persistent
    {
        PointerV constants;
        CodePointer sched, fault;
        Heap heap;
        Program *program;
    } _p;

    struct Transient
    {
        PointerV globals, frame, entry_frame, ifl;
        bool mask, interrupted;
        std::unordered_set< GenericPointer > cfl_visited;

    } _t;

    Context( Program &p ) { _p.program = &p; }

    Program &program() { return *_p.program; }
    Heap &heap() { return _p.heap; }
    PointerV frame() { return _t.frame; }
    PointerV globals() { return _t.globals; }
    PointerV constants() { return _p.constants; }

    void frame( PointerV p ) { _t.frame = p; }
    void globals( PointerV v ) { _t.globals = v; }
    void constants( PointerV v ) { _p.constants = v; }

    void push( PointerV ) {}

    template< typename X, typename... Args >
    void push( PointerV p, X x, Args... args )
    {
        heap().write_shift( p, x );
        push( p, args... );
    }

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        int datasz = program().function( pc ).datasize;
        auto frameptr = heap().make( datasz + 2 * PointerBytes );
        frame( frameptr );
        if ( parent.cooked().null() )
            _t.entry_frame = _t.frame;
        heap().write_shift( frameptr, PointerV( pc ) );
        heap().write_shift( frameptr, parent );
        push( frameptr, args... );
    }

    bool set_interrupted( bool i )
    {
        bool rv = _t.interrupted;
        _t.cfl_visited.clear();
        _t.interrupted = i;
        return rv;
    }

    void cfl_interrupt( CodePointer pc )
    {
        if ( _t.cfl_visited.count( pc ) )
            set_interrupted( true );
        else
            _t.cfl_visited.insert( pc );
    }

    void check_interrupt()
    {
        if ( _t.mask || !_t.interrupted )
            return;
        if ( !_t.ifl.cooked().null() )
            heap().write( _t.ifl.cooked(), _t.frame );
        frame( _t.entry_frame );
        _t.ifl = PointerV();
        _t.mask = true;
        _t.interrupted = false;
    }

    virtual void doublefault() = 0;

    void fault( Fault f, PointerV frame, CodePointer pc )
    {
        if ( _p.fault.null() )
            doublefault();
        else
            enter( _p.fault, _t.frame, value::Int< 32 >( f ), frame, PointerV( pc ) );
    }

    bool mask( bool n )  { bool o = _t.mask; _t.mask = n; return o; }
    bool mask() { return _t.mask; }

    CodePointer sched() { return _p.sched; }
    bool sched( CodePointer p )
    {
        if ( !_p.sched.null() )
            return false;
        _p.sched = p;
        return true;
    }

    bool fault_handler( CodePointer p )
    {
        if ( !_p.fault.null() )
            return false;
        _p.fault = p;
        return true;
    }
    CodePointer fault_handler() { return _p.fault; }

    bool isEntryFrame( HeapPointer fr ) { return HeapPointer( _t.entry_frame.cooked() ) == fr; }
    void setIfl( PointerV p ) { _t.ifl = p; }
};

}
}
