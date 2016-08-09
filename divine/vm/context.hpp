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

namespace divine {
namespace vm {

using Fault = ::_VM_Fault;

template< typename _Heap >
struct Context
{
    using Heap = _Heap;

    using HeapPointer = typename Heap::Pointer;
    using PointerV = typename Heap::PointerV;

    Heap _heap;
    Program &_program;
    PointerV _constants, _globals, _frame, _entry_frame, _ifl;
    CodePointer _sched, _fault;
    bool _mask;

    Context( Program &p ) : _program( p ) {}

    Heap &heap() { return _heap; }
    PointerV frame() { return _frame; }
    void frame( PointerV p ) { _frame = p; }
    PointerV globals() { return _globals; }
    PointerV constants() { return _constants; }

    void globals( PointerV v ) { _globals = v; }
    void constants( PointerV v ) { _constants = v; }

    void push( PointerV ) {}

    template< typename X, typename... Args >
    void push( PointerV p, X x, Args... args )
    {
        _heap.write_shift( p, x );
        push( p, args... );
    }

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        int datasz = _program.function( pc ).datasize;
        auto frameptr = _heap.make( datasz + 2 * PointerBytes );
        _frame = frameptr;
        if ( parent.cooked() == nullPointer() )
            _entry_frame = _frame;
        _heap.write_shift( frameptr, PointerV( pc ) );
        _heap.write_shift( frameptr, parent );
        push( frameptr, args... );
    }

    void interrupt()
    {
        HeapPointer ifl = _ifl.cooked();
        if ( _ifl.cooked() != nullPointer() )
            _heap.write( _ifl.cooked(), _frame );
        _frame = _entry_frame;
        _ifl = PointerV();
        _mask = true;
    }

    virtual void doublefault() = 0;

    void fault( Fault f, PointerV frame, CodePointer pc )
    {
        if ( _fault == CodePointer( nullPointer() ) )
            doublefault();
        else
            enter( _fault, _frame, value::Int< 32 >( f ), frame, PointerV( pc ) );
    }

    bool mask( bool n )  { bool o = _mask; _mask = n; return o; }
    bool mask() { return _mask; }

    bool isEntryFrame( HeapPointer fr ) { return HeapPointer( _entry_frame.cooked() ) == fr; }
    void setSched( CodePointer p ) { _sched = p; }
    void setFault( CodePointer p ) { _fault = p; }
    void setIfl( PointerV p ) { _ifl = p; }
};

}
}
