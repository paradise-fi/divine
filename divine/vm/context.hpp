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

namespace llvm { class Value; }

namespace divine {
namespace vm {

using Fault = ::_VM_Fault;

struct TraceText { GenericPointer text; };
struct TraceSchedChoice { value::Pointer list; };
struct TraceSchedInfo { int pid; int tid; };
struct TraceStateType { llvm::Value *stateptr; };
struct TraceInfo { GenericPointer text; };

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

    Program *_program;
    Heap _heap;
    std::unordered_set< GenericPointer > _cfl_visited;
    std::unordered_set< int > _mem_loads;

    template< typename Ctx >
    void load( const Ctx &ctx )
    {
        for ( int i = 0; i < _VM_CR_Last; ++i )
        {
            auto cr = _VM_ControlRegister( i );
            if ( cr == _VM_CR_Flags || cr == _VM_CR_User2 )
                set( cr, ctx.get( cr ).integer );
            else
                set( cr, ctx.get( cr ).pointer );
        }
        _heap = ctx.heap();
    }

    void load( typename Heap::Snapshot snap )
    {
        _cfl_visited.clear();
        _mem_loads.clear();
        _heap.restore( snap );
        flush_ptr2i();
    }

    void reset() { _heap.reset(); }

    template< typename I >
    int choose( int, I, I ) { return 0; }

    void set( _VM_ControlRegister r, uint64_t v ) { _reg[ r ].integer = v; }
    void set( _VM_ControlRegister r, GenericPointer v )
    {
        _reg[ r ].pointer = v;
        if ( r <= _VM_CR_Frame )
            _ptr2i[ r ] = v.null() ? HeapInternal() : _heap.ptr2i( v );
    }

    Register get( _VM_ControlRegister r ) const { return _reg[ r ]; }
    Register get( Location l ) const { ASSERT_LT( l, Program::Slot::Invalid ); return _reg[ l ]; }
    Register &ref( _VM_ControlRegister r ) { return _reg[ r ]; }

    HeapInternal ptr2i( Location l )
    {
        ASSERT_LT( l, Program::Slot::Invalid );
        ASSERT_EQ( _heap.ptr2i( _reg[ l ].pointer ), _ptr2i[ l ] );
        return _ptr2i[ l ];
    }
    HeapInternal ptr2i( _VM_ControlRegister r ) { return ptr2i( Location( r ) ); }

    void ptr2i( Location l, HeapInternal i ) { _ptr2i[ l ] = i; }
    void ptr2i( _VM_ControlRegister r, HeapInternal i ) { _ptr2i[ r ] = i; }
    void flush_ptr2i()
    {
        for ( int i = 0; i <= _VM_CR_Frame; ++i )
            _ptr2i[ i ] = _heap.ptr2i( get( Location( i ) ).pointer );
    }

    Context( Program &p ) : _program( &p ) {}
    Context( Program &p, const Heap &h ) : _program( &p ), _heap( h ) {}
    virtual ~Context() { }

    Program &program() { return *_program; }
    Heap &heap() { return _heap; }
    const Heap &heap() const { return _heap; }
    HeapPointer frame() { return get( _VM_CR_Frame ).pointer; }
    HeapPointer globals() { return get( _VM_CR_Globals ).pointer; }
    HeapPointer constants() { return get( _VM_CR_Constants ).pointer; }

    typename Heap::Snapshot snapshot()
    {
        auto rv = _heap.snapshot();
        flush_ptr2i();
        return rv;
    }

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
        auto frameptr = heap().make( program().function( pc ).framesize, 16 );
        set( _VM_CR_Frame, frameptr.cooked() );
        set( _VM_CR_PC, pc );
        heap().write_shift( frameptr, PointerV( pc ) );
        heap().write_shift( frameptr, parent );
        push( frameptr, args... );
    }

    bool set_interrupted( bool i )
    {
        auto &fl = ref( _VM_CR_Flags ).integer;
        bool rv = fl & _VM_CF_Interrupted;
        _cfl_visited.clear();
        _mem_loads.clear();
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

    void leave( CodePointer pc )
    {
        std::unordered_set< GenericPointer > pruned;
        for ( auto check : _cfl_visited )
            if ( check.object() != pc.function() )
                pruned.insert( check );
        std::swap( pruned, _cfl_visited );
    }

    void mem_interrupt( GenericPointer ptr, int type )
    {
        if ( ptr.type() == PointerType::Const )
            return;
        if ( type == _VM_MAT_Load )
        {
            if ( _mem_loads.count( ptr.object() ) )
                set_interrupted( true );
            else
                _mem_loads.insert( ptr.object() );
        }
        else
            set_interrupted( true );
    }

    template< typename Eval >
    void check_interrupt( Eval &eval )
    {
        if ( mask() || ( ref( _VM_CR_Flags ).integer & _VM_CF_Interrupted ) == 0 )
            return;
        if( ref( _VM_CR_Flags ).integer & _VM_CF_KernelMode )
        {
            eval.fault( _VM_F_Control ) << " illegal interrupt in kernel mode";
            return;
        }
        sync_pc();
        auto interrupted = get( _VM_CR_Frame ).pointer;
        set( _VM_CR_Frame, get( _VM_CR_IntFrame ).pointer );
        set( _VM_CR_IntFrame, interrupted );
        PointerV pc;
        heap().read( frame(), pc );
        set( _VM_CR_PC, pc.cooked() );
        set_interrupted( false );
        ref( _VM_CR_Flags ).integer |= _VM_CF_Mask | _VM_CF_KernelMode;
    }

    virtual void trace( TraceText ) {} // fixme?
    virtual void trace( TraceSchedInfo ) { NOT_IMPLEMENTED(); }
    virtual void trace( TraceSchedChoice ) { NOT_IMPLEMENTED(); }
    virtual void trace( TraceStateType ) { NOT_IMPLEMENTED(); }
    virtual void trace( TraceInfo ) { NOT_IMPLEMENTED(); }

    virtual void doublefault()
    {
        _heap.rollback();
        flush_ptr2i();
        ref( _VM_CR_Flags ).integer |= _VM_CF_Error;
        set( _VM_CR_Frame, nullPointer() );
    }

    void fault( Fault f, HeapPointer frame, CodePointer pc )
    {
        auto fh = get( _VM_CR_FaultHandler ).pointer;
        if ( fh.null() )
            doublefault();
        else
            enter( fh, PointerV( get( _VM_CR_Frame ).pointer ),
                   value::Int< 32 >( f ), PointerV( frame ), PointerV( pc ), nullPointerV() );
    }

    bool mask( bool n )
    {
        auto &fl = ref( _VM_CR_Flags ).integer;
        bool rv = fl & _VM_CF_Mask;
        fl &= ~_VM_CF_Mask;
        fl |= n ? _VM_CF_Mask : 0;
        return rv;
    }

    void sync_pc()
    {
        auto frame = get( _VM_CR_Frame ).pointer;
        if ( frame.null() )
            return;
        auto pc = get( _VM_CR_PC ).pointer;
        ptr2i( _VM_CR_Frame,
               heap().write( frame, PointerV( pc ), ptr2i( _VM_CR_Frame ) ) );
    }

    bool mask() { return ref( _VM_CR_Flags ).integer & _VM_CF_Mask; }
};

template< typename Program, typename _Heap >
struct ConstContext : Context< Program, _Heap >
{
    void setup( int gds, int cds )
    {
        this->set( _VM_CR_Constants, this->heap().make( cds ).cooked() );
        if ( gds )
            this->set( _VM_CR_Globals, this->heap().make( gds ).cooked() );
    }

    ConstContext( Program &p ) : Context< Program, _Heap >( p ) {}
    ConstContext( Program &p, const _Heap &h ) : Context< Program, _Heap >( p, h ) {}
};

}
}
