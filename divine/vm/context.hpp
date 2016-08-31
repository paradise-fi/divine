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

#include <runtime/divine.h>

#include <unordered_set>

namespace divine {
namespace vm {

using Fault = ::_VM_Fault;

struct TraceText { value::Pointer text; };
struct TraceSchedChoice { value::Pointer list; };
struct TraceSchedInfo { int pid; int tid; };

template< typename _Program, typename _Heap >
struct Context
{
    using Program = _Program;
    using Heap = _Heap;
    using PointerV = value::Pointer;
    using HeapInternal = typename Heap::Internal;
    using Location = typename Program::Slot::Location;

    union Register
    {
        GenericPointer pointer;
        uint64_t integer;
        Register() : integer( 0 ) {}
    } _reg[ _VM_CR_Last ];

    /* indexed by _VM_ControlRegister */
    HeapInternal _ptr2i[ _VM_CR_Frame + 1 ];

    Heap _heap;
    Program *_program;
    std::unordered_set< GenericPointer > _cfl_visited;

    template< typename Ctx >
    void load( const Ctx &ctx )
    {
        for ( int i = 0; i < _VM_CR_Last; ++i )
            if ( i == _VM_CR_Flags || i == _VM_CR_User2 )
                set( i, ctx.get( i ).integer );
            else
                set( i, ctx.get( i ).pointer );
    }

    template< typename I >
    int choose( int, I, I ) { return 0; }

    void set( _VM_ControlRegister r, uint64_t v ) { _reg[ r ].integer = v; }
    void set( _VM_ControlRegister r, GenericPointer v )
    {
        _reg[ r ].pointer = v;
        if ( r <= _VM_CR_Frame )
            _ptr2i[ r ] = v.null() ? HeapInternal() : _heap.ptr2i( v );
    }

    Register get( _VM_ControlRegister r ) { return _reg[ r ]; }
    Register get( Location l ) { ASSERT_LT( l, Program::Slot::Invalid ); return _reg[ l ]; }
    uint64_t &ref( _VM_ControlRegister r ) { return _reg[ r ].integer; }

    HeapInternal ptr2i( _VM_ControlRegister r ) { return _ptr2i[ r ]; }
    HeapInternal ptr2i( Location l ) { ASSERT_LT( l, Program::Slot::Invalid ); return _ptr2i[ l ]; }
    void ptr2i( Location l, HeapInternal i ) { _ptr2i[ l ] = i; }
    void ptr2i( _VM_ControlRegister r, HeapInternal i ) { _ptr2i[ r ] = i; }

    Context( Program &p ) : _program( &p ) {}
    Context( Program &p, const Heap &h ) : _program( &p ), _heap( h ) {}

    Program &program() { return *_program; }
    Heap &heap() { return _heap; }
    HeapPointer frame() { return get( _VM_CR_Frame ).pointer; }
    HeapPointer globals() { return get( _VM_CR_Globals ).pointer; }
    HeapPointer constants() { return get( _VM_CR_Constants ).pointer; }

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
        auto frameptr = heap().make( program().function( pc ).framesize );
        set( _VM_CR_Frame, frameptr.cooked() );
        heap().write_shift( frameptr, PointerV( pc ) );
        heap().write_shift( frameptr, parent );
        push( frameptr, args... );
    }

    bool set_interrupted( bool i )
    {
        auto &fl = ref( _VM_CR_Flags );
        bool rv = fl & _VM_CF_Interrupted;
        _cfl_visited.clear();
        fl &= ~_VM_CF_Interrupted;
        fl |= i ? _VM_CF_Interrupted : 0;
        return rv;
    }

    void cfl_interrupt( CodePointer pc )
    {
        if ( _cfl_visited.count( pc ) )
            set_interrupted( true );
        else
            _cfl_visited.insert( pc );
    }

    void check_interrupt()
    {
        if ( mask() || ( ref( _VM_CR_Flags ) & _VM_CF_Interrupted ) == 0 )
            return;
        ASSERT_EQ( ref( _VM_CR_Flags ) & _VM_CF_KernelMode, 0 );
        auto interrupted = get( _VM_CR_Frame ).pointer;
        set( _VM_CR_Frame, get( _VM_CR_IntFrame ).pointer );
        set( _VM_CR_IntFrame, interrupted );
        mask( true );
        set_interrupted( false );
        ref( _VM_CR_Flags ) |= _VM_CF_KernelMode;
    }

    virtual void trace( TraceText tt ) {} // fixme?
    virtual void trace( TraceSchedInfo ) { NOT_IMPLEMENTED(); }
    virtual void trace( TraceSchedChoice ) { NOT_IMPLEMENTED(); }

    virtual void doublefault()
    {
        /* TODO trace? */
        set( _VM_CR_Frame, nullPointer() );
    }

    void fault( Fault f, HeapPointer frame, CodePointer pc )
    {
        auto fh = get( _VM_CR_FaultHandler ).pointer;
        if ( fh.null() )
            doublefault();
        else
            enter( fh, PointerV( get( _VM_CR_Frame ).pointer ),
                   value::Int< 32 >( f ), PointerV( frame ), PointerV( pc ) );
    }

    bool mask( bool n )
    {
        auto &fl = ref( _VM_CR_Flags );
        bool rv = fl & _VM_CF_Mask;
        fl &= ~_VM_CF_Mask;
        fl |= n ? _VM_CF_Mask : 0;
        return rv;
    }

    bool mask() { return ref( _VM_CR_Flags ) & _VM_CF_Mask; }
};

}
}
