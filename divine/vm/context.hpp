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
#include <divine/vm/types.hpp>
#include <divine/vm/divm.h>

#include <brick-data>
#include <brick-types>

#include <unordered_set>

namespace llvm { class Value; }

namespace divine::vm
{

struct TraceSchedChoice { value::Pointer list; };

template< typename _Program, typename _Heap >
struct Context
{
    using Program = _Program;
    using Heap = _Heap;
    using PointerV = value::Pointer;
    using HeapInternal = typename Heap::Internal;
    using Location = typename Program::Slot::Location;
    using Snapshot = typename Heap::Snapshot;

    union Register
    {
        GenericPointer pointer;
        uint64_t integer;
        Register() : integer( 0 ) {}
    } _reg[ _VM_CR_Last ], _debug_reg[ _VM_CR_Last ];

    /* indexed by _VM_ControlRegister */
    HeapInternal _ptr2i[ _VM_CR_Frame + 1 ];

    Program *_program;
    Heap _heap;
    std::vector< Interrupt > _interrupts;

    uint32_t _instruction_counter;
    int _debug_depth = 0;
    bool _debug_allowed = false, _track_mem = false;
    TraceDebugPersist _debug_persist;
    Snapshot _debug_snap;

    using MemMap = brick::data::IntervalSet< GenericPointer >;
    std::vector< std::unordered_set< GenericPointer > > _cfl_visited;
    MemMap _mem_loads, _mem_stores, _crit_loads, _crit_stores;

    bool debug_allowed() { return _debug_allowed; }
    bool debug_mode() { return _reg[ _VM_CR_Flags ].integer & _VM_CF_DebugMode; }
    void enable_debug() { _debug_allowed = true; }
    void track_memory( bool b ) { _track_mem = b; }

    template< typename Ctx >
    void load( const Ctx &ctx )
    {
        clear();
        for ( int i = 0; i < _VM_CR_Last; ++i )
        {
            auto cr = _VM_ControlRegister( i );
            if ( cr == _VM_CR_Flags || cr == _VM_CR_User2 )
                set( cr, ctx.get( cr ).integer );
            else
                set( cr, ctx.get( cr ).pointer );
        }
        ASSERT( !debug_mode() );
        _heap = ctx.heap();
    }

    void reset_interrupted()
    {
        _cfl_visited.clear();
        _cfl_visited.emplace_back();
        set_interrupted( false );
    }

    void clear()
    {
        _interrupts.clear();
        reset_interrupted();
        flush_ptr2i();
        set( _VM_CR_User1, 0 );
        set( _VM_CR_User2, 0 );
        set( _VM_CR_User3, 0 );
        set( _VM_CR_User4, 0 );
        set( _VM_CR_ObjIdShuffle, 0 );
        _mem_loads.clear();
        _mem_stores.clear();
        _crit_loads.clear();
        _crit_stores.clear();
        _instruction_counter = 0;
    }

    void load( Snapshot snap ) { _heap.restore( snap ); clear(); }
    void reset() { _heap.reset(); clear(); }

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

    void ptr2i( Location l, HeapInternal i ) { if ( i ) _ptr2i[ l ] = i; else flush_ptr2i(); }
    void ptr2i( _VM_ControlRegister r, HeapInternal i ) { ptr2i( Location( r ), i ); }
    void flush_ptr2i()
    {
        for ( int i = 0; i <= _VM_CR_Frame; ++i )
            _ptr2i[ i ] = _heap.ptr2i( get( Location( i ) ).pointer );
    }

    Context( Program &p ) : Context( p, Heap() ) {}
    Context( Program &p, const Heap &h ) : _program( &p ), _heap( h )
    {
        for ( int i = 0; i < _VM_CR_Last; ++i )
            _reg[ i ].integer = 0;
    }
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

    void push( typename Program::Function &f, int i, HeapPointer )
    {
        ASSERT_EQ( f.argcount + f.vararg , i );
    }

    template< typename X, typename... Args >
    void push( typename Program::Function &f, int i, HeapPointer p, X x, Args... args )
    {
        heap().write( p + f.instructions[ i ].result().offset, x );
        push( f, i + 1, p, args... );
    }

    template< typename H, typename F >
    static auto with_snap( H &heap, F f, brick::types::Preferred )
        -> decltype( heap.is_shared( heap.snapshot() ), void( 0 ) )
    {
        f( heap );
    }

    template< typename H, typename F >
    static void with_snap( H &, F, brick::types::NotPreferred )
    {
        /* nothing: debug mode does not need a rollback in 'run' */
    }

    template< typename F >
    void with_snap( F f )
    {
        with_snap( heap(), f, brick::types::Preferred() );
        flush_ptr2i();
    }

    bool enter_debug();
    void leave_debug();

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        auto &f = program().function( pc );
        auto frameptr = heap().make( f.framesize, 16 ).cooked();
        set( _VM_CR_Frame, frameptr );
        set( _VM_CR_PC, pc );
        heap().write( frameptr, PointerV( pc ) );
        heap().write( frameptr + PointerBytes, parent );
        push( f, 0, frameptr, args... );
        entered( pc );
    }

    bool set_interrupted( bool i )
    {
        auto &fl = ref( _VM_CR_Flags ).integer;
        bool rv = fl & _VM_CF_Interrupted;
        fl &= ~_VM_CF_Interrupted;
        fl |= i ? _VM_CF_Interrupted : 0;
        return rv;
    }

    void cfl_interrupt( CodePointer pc )
    {
        if( in_kernel() || debug_mode() )
            return;

        if ( _cfl_visited.back().count( pc ) )
            trigger_interrupted( Interrupt::Cfl, pc );
        else
            _cfl_visited.back().insert( pc );
    }

    void entered( CodePointer )
    {
        if ( debug_mode() )
            ++ _debug_depth;
        else
            _cfl_visited.emplace_back();
    }

    void left( CodePointer )
    {
        if ( debug_mode() )
        {
            ASSERT_LEQ( 1, _debug_depth );
            -- _debug_depth;
            if ( !_debug_depth )
                leave_debug();
        }
        else
        {
            _cfl_visited.pop_back();
            if ( _cfl_visited.empty() ) /* more returns than calls could happen along an edge */
                _cfl_visited.emplace_back();
        }
    }

    void trigger_interrupted( Interrupt::Type t, CodePointer pc )
    {
        if ( !set_interrupted( true ) )
            _interrupts.push_back( Interrupt{ t, _instruction_counter, pc } );
    }

    void mem_interrupt( CodePointer pc, GenericPointer ptr, int size, int type )
    {
        if( in_kernel() || debug_mode() )
            return;

        if ( ptr.heap() && !heap().shared( ptr ) )
            return;

        auto start = ptr;
        if ( start.heap() )
            start.type( PointerType::Heap );
        auto end = start;
        end.offset( start.offset() + size );

        if ( type == _VM_MAT_Load || type == _VM_MAT_Both )
        {
            if ( _crit_loads.intersect( start, end ) )
                trigger_interrupted( Interrupt::Mem, pc );
            else if ( _track_mem )
                _mem_loads.insert( start, end );
        }

        if ( type == _VM_MAT_Store || type == _VM_MAT_Both )
        {
            if ( _crit_stores.intersect( start, end ) )
                trigger_interrupted( Interrupt::Mem, pc );
            else if ( _track_mem )
                _mem_stores.insert( start, end );
        }
    }

    void count_instruction()
    {
        if ( !debug_mode() )
            ++ _instruction_counter;
    }

    template< typename Eval >
    bool check_interrupt( Eval &eval )
    {
        if ( mask() || ( ref( _VM_CR_Flags ).integer & _VM_CF_Interrupted ) == 0 )
            return false;

        if( in_kernel() || debug_mode() )
        {
            eval.fault( _VM_F_Control ) << "illegal interrupt in kernel or debug mode";
            return false;
        }

        sync_pc();
        auto interrupted = get( _VM_CR_Frame ).pointer;
        set( _VM_CR_Frame, get( _VM_CR_IntFrame ).pointer );
        set( _VM_CR_IntFrame, interrupted );
        PointerV pc;
        heap().read( frame(), pc );
        set( _VM_CR_PC, pc.cooked() );
        reset_interrupted();
        ref( _VM_CR_Flags ).integer |= _VM_CF_Mask | _VM_CF_KernelMode;
        return true;
    }

    virtual void trace( TraceDebugPersist t ) { _debug_persist = t; }
    virtual void trace( TraceText tt ) { trace( heap().read_string( tt.text ) ); }
    virtual void trace( TraceSchedInfo ) {}
    virtual void trace( TraceSchedChoice ) {}
    virtual void trace( TraceTaskID ) {}
    virtual void trace( TraceStateType ) {}
    virtual void trace( TraceTypeAlias ) {}
    virtual void trace( TraceInfo ) {}
    virtual void trace( TraceAssume ) {}
    virtual void trace( std::string ) {}

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
        if ( debug_mode() )
        {
            trace( "W: cannot handle a fault in debug mode (abandoned)" );
            _debug_depth = 0; /* short-circuit */
            leave_debug();
        }
        else if ( fh.null() )
        {
            trace( "FATAL: no fault handler installed" );
            doublefault();
        }
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
    bool in_kernel() { return ref( _VM_CR_Flags ).integer & _VM_CF_KernelMode; }

    bool flags_any( uint64_t f ) { return ref( _VM_CR_Flags ).integer & f; }
    bool flags_all( uint64_t f ) { return ( ref( _VM_CR_Flags ).integer & f ) == f; }
    void flags_set( uint64_t clear, uint64_t set )
    {
        ref( _VM_CR_Flags ).integer &= ~clear;
        ref( _VM_CR_Flags ).integer |=  set;
    }
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
