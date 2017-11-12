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
#include <divine/vm/heap.hpp>
#include <divine/vm/divm.h>

#include <brick-data>
#include <brick-types>

#include <unordered_set>

namespace llvm { class Value; }

namespace divine {
namespace vm {

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

static inline std::ostream &operator<<( std::ostream &o, Interrupt i )
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

static inline std::ostream &operator<<( std::ostream &o, Choice i )
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
struct TraceSchedChoice { value::Pointer list; };
struct TraceSchedInfo { int pid; int tid; };
struct TraceStateType { CodePointer pc; };
struct TraceInfo { GenericPointer text; };
struct TraceAlg { brick::data::SmallVector< divine::vm::GenericPointer > args; };
struct TraceTypeAlias { CodePointer pc; GenericPointer alias; };
struct TraceDebugPersist { GenericPointer ptr; };

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
    } _reg[ _VM_CR_Last ], _debug_reg[ _VM_CR_Last ];

    /* indexed by _VM_ControlRegister */
    HeapInternal _ptr2i[ _VM_CR_Frame + 1 ];

    Program *_program;
    Heap _heap;
    std::vector< Interrupt > _interrupts;

    uint32_t _instruction_counter;
    int _debug_depth = 0;
    bool _debug_allowed = false;
    TraceDebugPersist _debug_persist;
    typename Heap::Snapshot _debug_snap;

    using MemMap = brick::data::IntervalSet< GenericPointer >;
    std::vector< std::unordered_set< GenericPointer > > _cfl_visited;
    MemMap _mem_loads, _mem_stores, _crit_loads, _crit_stores;

    bool debug_allowed() { return _debug_allowed; }
    bool debug_mode() { return _reg[ _VM_CR_Flags ].integer & _VM_CF_DebugMode; }
    void enable_debug() { _debug_allowed = true; }

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
        set( _VM_CR_ObjIdShuffle, 0 );
        _mem_loads.clear();
        _mem_stores.clear();
        _instruction_counter = 0;
    }

    void load( typename Heap::Snapshot snap ) { _heap.restore( snap ); clear(); }
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

    void push( PointerV ) {}

    template< typename X, typename... Args >
    void push( PointerV p, X x, Args... args )
    {
        heap().write_shift( p, x );
        push( p, args... );
    }

    template< typename H, typename F >
    static auto with_snap( H &heap, F f, brick::types::Preferred )
        -> decltype( heap.snapshot(), void( 0 ) )
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

    bool enter_debug()
    {
        if ( !debug_allowed() )
            -- _instruction_counter; /* dbg.call does not count */

        if ( debug_allowed() && !debug_mode() )
        {
            -- _instruction_counter;
            ASSERT( !_debug_depth );
            std::copy( _reg, _reg + _VM_CR_Last, _debug_reg );
            _reg[ _VM_CR_Flags ].integer |= _VM_CF_DebugMode;
            with_snap( [&]( auto &h ) { _debug_snap = h.snapshot(); } );
            return true;
        }
        else
            return false;
    }

    void leave_debug()
    {
        ASSERT( debug_allowed() );
        ASSERT( debug_mode() );
        ASSERT( !_debug_depth );
        std::copy( _debug_reg, _debug_reg + _VM_CR_Last, _reg );
        if ( _debug_persist.ptr.null() )
            with_snap( [&]( auto &h ) { h.restore( _debug_snap ); } );
        else
        {
            Heap from = heap();
            with_snap( [&]( auto &h ) { h.restore( _debug_snap ); } );
            vm::heap::clone( from, heap(), _debug_persist.ptr,
                             vm::heap::CloneType::SkipWeak, true );
            _debug_persist.ptr = nullPointer();
        }
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
            else
                _mem_loads.insert( start, end );
        }

        if ( type == _VM_MAT_Store || type == _VM_MAT_Both )
        {
            if ( _crit_stores.intersect( start, end ) )
                trigger_interrupted( Interrupt::Mem, pc );
            else
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

        ASSERT( !debug_mode() );

        if( in_kernel() )
        {
            eval.fault( _VM_F_Control ) << " illegal interrupt in kernel mode";
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
    virtual void trace( TraceStateType ) {}
    virtual void trace( TraceTypeAlias ) {}
    virtual void trace( TraceInfo ) {}
    virtual void trace( TraceAlg ) {}
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
        if ( fh.null() || debug_mode() )
        {
            if ( debug_mode() )
            {
                trace( "FATAL: cannot handle a fault during dbg.call" );
                _debug_depth = 0; /* short-circuit */
                leave_debug();
            }
            if ( fh.null() )
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
