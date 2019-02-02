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
#include <divine/vm/memory.hpp>
#include <divine/vm/types.hpp>
#include <divine/vm/divm.h>

#include <brick-data>
#include <brick-types>

#include <unordered_set>

namespace llvm { class Value; }

namespace divine::vm
{

struct TraceSchedChoice { value::Pointer list; };

using Location = _VM_Operand::Location;
using PtrRegister = GenericPointer;
using IntRegister = uint64_t;

struct LoopTrack
{
    std::vector< std::unordered_set< GenericPointer > > loops;

    void entered( CodePointer )
    {
        loops.emplace_back();
    }

    void left( CodePointer )
    {
        loops.pop_back();
        if ( loops.empty() ) /* more returns than calls could happen along an edge */
            loops.emplace_back();
    }

    bool test_loop( CodePointer pc, int /* TODO */ )
    {
        /* TODO track the counter value too */
        if ( loops.back().count( pc ) )
            return true;
        else
            loops.back().insert( pc );

        return false;
    }

};

/* state of a computation */
struct State
{
    std::array< PtrRegister, _VM_CR_PtrCount > ptr;
    std::array< PtrRegister, _VM_CR_UserCount > user;
    IntRegister flags = 0, objid_shuffle = 0;
    uint32_t instruction_counter = 0;
};

struct Registers
{
    State _state; /* TODO make this a ref */
    PtrRegister _pc;
    PtrRegister _fault_handler, _state_ptr, _scheduler;

    void set( _VM_ControlRegister r, uint64_t v )
    {
        ASSERT( r == _VM_CR_Flags || r == _VM_CR_ObjIdShuffle );
        if ( r == _VM_CR_Flags )
            _state.flags = v;
        else
            _state.objid_shuffle = v;
    }

    PtrRegister &_ref_ptr( _VM_ControlRegister r )
    {
        ASSERT( r != _VM_CR_ObjIdShuffle && r != _VM_CR_Flags );

        if ( r < _VM_CR_PtrCount )
            return _state.ptr[ r ];
        else if ( r < _VM_CR_PtrCount + _VM_CR_UserCount )
            return _state.user[ r - _VM_CR_User1 ];
        else if ( r == _VM_CR_Scheduler )
            return _scheduler;
        else if ( r == _VM_CR_FaultHandler )
            return _fault_handler;
        else if ( r == _VM_CR_State )
            return _state_ptr;
        else if ( r == _VM_CR_PC )
            return _pc;
        else __builtin_unreachable();
    }

    PtrRegister _ref_ptr( _VM_ControlRegister r ) const
    {
        return const_cast< Registers * >( this )->_ref_ptr( r );
    }

    void set( _VM_ControlRegister r, GenericPointer v )
    {
        _ref_ptr( r ) = v;
    }

    PtrRegister get_ptr( _VM_ControlRegister r ) const
    {
        return _ref_ptr( r );
        NOT_IMPLEMENTED();
    }

    IntRegister get_int( _VM_ControlRegister r ) const
    {
        if ( r == _VM_CR_Flags )
            return _state.flags;
        if ( r == _VM_CR_ObjIdShuffle )
            return _state.objid_shuffle;
        NOT_IMPLEMENTED();
    }

    uint32_t instruction_count() { return _state.instruction_counter; }
    void instruction_count( uint32_t v ) { return _state.instruction_counter = v; }

    void pc( PtrRegister pc ) { _pc = pc; }

    PtrRegister pc() { return _pc; }
    PtrRegister frame() { return _state.ptr[ _VM_CR_Frame ]; }
    PtrRegister globals() { return _state.ptr[ _VM_CR_Globals ]; }
    PtrRegister constants() { return _state.ptr[ _VM_CR_Constants ]; }
    IntRegister &objid_shuffle() { return _state.objid_shuffle; }

    PtrRegister fault_handler() { return _fault_handler; }
    PtrRegister state_ptr() { return _state_ptr; }
    PtrRegister scheduler() { return _scheduler; }

    bool in_kernel() { return _state.flags & _VM_CF_KernelMode; }

    uint64_t flags() { return _state.flags; }
    bool flags_any( uint64_t f ) { return _state.flags & f; }
    bool flags_all( uint64_t f ) { return ( _state.flags & f ) == f; }
    void flags_set( uint64_t clear, uint64_t set )
    {
        _state.flags &= ~clear;
        _state.flags |=  set;
    }

};

struct TracingInterface
{
    virtual void trace( TraceText tt ) {}
    virtual void trace( TraceDebugPersist ) {}
    virtual void trace( TraceSchedInfo ) {}
    virtual void trace( TraceSchedChoice ) {}
    virtual void trace( TraceTaskID ) {}
    virtual void trace( TraceStateType ) {}
    virtual void trace( TraceTypeAlias ) {}
    virtual void trace( TraceInfo ) {}
    virtual void trace( TraceAssume ) {}
    virtual void trace( TraceLeakCheck ) {}
    virtual void trace( std::string ) {}
    virtual ~TracingInterface() {}
};

struct NoTracing : Registers, TracingInterface
{
    bool enter_debug() {}
    void leave_debug() {}
    bool debug_allowed() { return false; }
    bool debug_mode() { return false; }

    /* an interface for debug mode implementation */
    void debug_save() {}
    void debug_restore() {}

    bool test_loop( CodePointer, int )
    {
        return !this->flags_all( _VM_CF_IgnoreLoop );
    }

    bool test_crit( CodePointer, GenericPointer, int, int )
    {
        return !this->flags_all( _VM_CF_IgnoreCrit );
    }

    void entered( CodePointer ) {}
    void left( CodePointer ) {}
};

struct Tracing : NoTracing
{
    State _debug_state;
    LoopTrack _loops;
    PtrRegister _debug_pc;

    int _debug_depth = 0;
    bool _debug_allowed = false;
    std::vector< Interrupt > _interrupts;

    bool debug_allowed() { return _debug_allowed; }
    bool debug_mode() { return flags_any( _VM_CF_DebugMode ); }
    void enable_debug() { _debug_allowed = true; }

    bool test_loop( CodePointer p, int i )
    {
        return NoTracing::test_loop( p, i ) && !debug_mode();
    }

    bool test_crit( CodePointer p, GenericPointer a, int s, int t )
    {
        return NoTracing::test_crit( p, a, s, t ) && !debug_mode();
    }

    bool enter_debug();
    void leave_debug();

    using Registers::set;
    void set( _VM_ControlRegister r, uint64_t v )
    {
        Registers::set( r, v );
        if ( _debug_depth )
            ASSERT( debug_mode() );
    }

    void entered( CodePointer f )
    {
        if ( debug_mode() )
            ++ _debug_depth;
        else
            _loops.entered( f );
    }

    void left( CodePointer f )
    {
        if ( debug_mode() )
        {
            ASSERT_LEQ( 1, _debug_depth );
            -- _debug_depth;
            if ( !_debug_depth )
                leave_debug();
        }
        else
            _loops.left( f );
    }

    virtual void debug_save() {}
    virtual void debug_restore() {}
};

template< typename Heap_ >
struct Memory
{
    using Heap = Heap_;
    using HeapInternal = typename Heap::Internal;
    using Snapshot = typename Heap::Snapshot;

    Heap _heap; /* TODO make this a reference */
    std::array< HeapInternal, _VM_CR_PtrCount > _ptr2i;

    Memory( Registers &regs, Heap h )
        : _heap( h )
    {
        flush_ptr2i( regs );

        if ( !regs.frame().null() )
        {
            PointerV pc;
            this->heap().read( regs.frame(), pc );
            regs.pc( pc.cooked() );
        }
    }

    Heap &heap() { return _heap; }
    const Heap &heap() const { return _heap; }

    void set( _VM_ControlRegister r, GenericPointer v )
    {
        if ( r < _VM_CR_PtrCount )
            _ptr2i[ r ] = v.null() ? HeapInternal() : _heap.ptr2i( v );
    }

    HeapInternal ptr2i( Registers &regs, Location l )
    {
        ASSERT_LT( l, _VM_Operand::Invalid );
        ASSERT_EQ( _heap.ptr2i( regs.get_ptr( _VM_ControlRegister( l ) ) ), _ptr2i[ l ] );
        return _ptr2i[ l ];
    }

    void ptr2i( Registers &regs, Location l, HeapInternal i )
    {
        if ( i )
            _ptr2i[ l ] = i;
        else
            flush_ptr2i( regs );
    }

    void flush_ptr2i( Registers &regs )
    {
        for ( int i = 0; i < _VM_CR_PtrCount; ++i )
            _ptr2i[ i ] = _heap.ptr2i( regs.get_ptr( _VM_ControlRegister( i ) ) );
    }

    void sync_pc( Registers &regs )
    {
        if ( regs.frame().null() )
            return;
        auto loc = _heap.loc( regs.frame(), ptr2i( regs, Location( _VM_CR_Frame ) ) );
        auto newfr = _heap.write( loc, PointerV( regs.pc() ) );
        ptr2i( regs, Location( _VM_CR_Frame ), newfr );
    }
};

struct CowMemory : Memory< vm::CowHeap >
{};

struct NoFault
{
    void doublefault() {}
    void fault( Fault, HeapPointer, CodePointer ) {}
};

struct NoChoice
{
    template< typename I >
    int choose( int, I, I ) { return 0; }
};

template< typename Reg, typename Mem >
struct ContextBase : Reg, Mem
{
    using HeapInternal = typename Mem::HeapInternal;

    ContextBase( typename Mem::Heap h )
        : Mem( *this, h )
    {}

    HeapInternal ptr2i( Location l ) { return Mem::ptr2i( *this, l ); }
    void ptr2i( Location l, HeapInternal i ) { return Mem::ptr2i( *this, l, i ); }
    void flush_ptr2i() { return Mem::flush_ptr2i( *this ); }
    void sync_pc() { return Mem::sync_pc( *this ); }

    typename Mem::Heap::Snapshot snapshot()
    {
        auto rv = this->_heap.snapshot();
        this->flush_ptr2i();
        return rv;
    }

    using Reg::set;
    void set( _VM_ControlRegister r, GenericPointer v )
    {
        Reg::set( r, v );
        Mem::set( r, v );
    }

    using Reg::trace;
    virtual void trace( TraceText tt ) { trace( this->heap().read_string( tt.text ) ); }
};

template< typename Heap >
struct TracingContext : ContextBase< Tracing, Memory< Heap > >, NoChoice
{
    using Base = ContextBase< Tracing, Memory< Heap > >;

    TracingContext( typename Base::Heap h )
        : Base( h )
    {}

    std::vector< HeapPointer > _debug_persist;
    typename Base::Snapshot _debug_snap;

    using Base::trace;
    virtual void trace( TraceDebugPersist t ) { _debug_persist.push_back( t.ptr ); }

    virtual void debug_save() override;
    virtual void debug_restore() override;
};

template< typename Ctx >
struct MakeFrame
{
    Ctx &_ctx;
    typename Ctx::Program::Function *_fun = nullptr;
    CodePointer _pc;
    bool _incremental;

    MakeFrame( Ctx &ctx, CodePointer pc, bool inc = false )
        : _ctx( ctx ), _pc( pc ), _incremental( inc )
    {
        if ( _pc.function() )
            _fun = &ctx.program().function( pc );
    }

    void push( int i, HeapPointer )
    {
        if ( _incremental )
        {
            if ( i == _fun->argcount + _fun->vararg )
                _incremental = false;
        }
        else
            ASSERT_EQ( _fun->argcount + _fun->vararg , i );
    }

    template< typename X, typename... Args >
    void push( int i, HeapPointer p, X x, Args... args )
    {
        _ctx.heap().write( p + _fun->instructions[ i ].result().offset, x );
        push( i + 1, p, args... );
    }

    template< typename... Args >
    void enter( PointerV parent, Args... args )
    {
        ASSERT( _fun );
        auto frameptr = _ctx.heap().make( _fun->framesize, _VM_PL_Code + 16 ).cooked();
        _ctx.set( _VM_CR_Frame, frameptr );
        _ctx.pc( _pc );
        _ctx.heap().write( frameptr, PointerV( _pc ) );
        _ctx.heap().write( frameptr + PointerBytes, parent );
        push( 0, frameptr, args... );
        _ctx.entered( _pc );
    }
};

template< typename Program_, typename Heap_ >
struct Context : TracingContext< Heap_ >
{
    using Program = Program_;
    using Heap = Heap_;
    using PointerV = value::Pointer;
    using Location = typename Program::Slot::Location;
    using Snapshot = typename Heap::Snapshot;
    using Base = TracingContext< Heap_ >;
    using Base::_heap;

    Program *_program;

    using Base::trace;
    virtual void trace( TraceLeakCheck ) override;

    bool _track_mem = false;

    using MemMap = brick::data::IntervalSet< GenericPointer >;

    void track_memory( bool b ) { _track_mem = b; }

    template< typename Ctx >
    void load( const Ctx &ctx )
    {
        clear();
        for ( int i = 0; i < _VM_CR_Last; ++i )
        {
            auto cr = _VM_ControlRegister( i );
            if ( cr == _VM_CR_Flags || cr == _VM_CR_ObjIdShuffle )
                this->set( cr, ctx.get_int( cr ) );
            else
                this->set( cr, ctx.get_ptr( cr ) );
        }
        ASSERT( !this->debug_mode() );
        this->_heap = ctx.heap();
    }

    void reset_interrupted()
    {
        this->_loops.loops.clear();
        this->_loops.loops.emplace_back();
    }

    virtual void clear()
    {
        this->_interrupts.clear();
        reset_interrupted();
        this->flush_ptr2i();
        this->set( _VM_CR_User1, GenericPointer() );
        this->set( _VM_CR_User2, GenericPointer() );
        this->set( _VM_CR_User3, GenericPointer() );
        this->set( _VM_CR_User4, GenericPointer() );
        this->set( _VM_CR_ObjIdShuffle, 0 );
        this->_state.instruction_counter = 0;
    }

    void load( Snapshot snap ) { _heap.restore( snap ); clear(); }
    void reset() { _heap.reset(); clear(); }

    Context( Program &p ) : Context( p, Heap() ) {}
    Context( Program &p, const Heap &h ) : Base( h ), _program( &p )
    {}

    virtual ~Context() { }

    Program &program() { return *_program; }

    HeapPointer frame() { return Base::frame(); }
    HeapPointer globals() { return Base::globals(); }
    HeapPointer constants() { return Base::constants(); }

    bool track_test( Interrupt::Type t, CodePointer pc )
    {
        // if ( _debug_allowed ) TODO
            this->_interrupts.push_back( Interrupt{ t, this->_state.instruction_counter, pc } );
        return true;
    }

    bool test_loop( CodePointer pc, int ctr )
    {
        if ( this->flags_all( _VM_CF_IgnoreLoop ) || this->debug_mode() )
            return false;

        if ( this->_loops.test_loop( pc, ctr ) )
            return track_test( Interrupt::Cfl, pc );
        else
            return false;
    }

    void count_instruction()
    {
        if ( !this->debug_mode() )
            ++ this->_state.instruction_counter;
    }

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        MakeFrame< Context > mkframe( *this, pc );
        mkframe.enter( parent, args... );
    }

    virtual void doublefault()
    {
        this->flags_set( 0, _VM_CF_Error );
        this->set( _VM_CR_Frame, nullPointer() );
    }

    void fault( Fault f, HeapPointer frame, CodePointer pc )
    {
        auto fh = this->fault_handler();
        if ( this->debug_mode() )
        {
            this->trace( "W: cannot handle a fault in debug mode (abandoned)" );
            this->_debug_depth = 0; /* short-circuit */
            this->leave_debug();
        }
        else if ( fh.null() )
        {
            this->trace( "FATAL: no fault handler installed" );
            doublefault();
        }
        else
            enter( fh, PointerV( this->frame() ),
                   value::Int< 32 >( f ), PointerV( frame ), PointerV( pc ) );
    }

};

template< typename Program_, typename Heap_ >
struct ConstContext : ContextBase< NoTracing, Memory< Heap_ > >, NoFault, NoChoice
{
    using Base = ContextBase< NoTracing, Memory< Heap_ > >;
    using Program = Program_;
    Program *_program;
    Program &program() { return *_program; }

    void setup( int gds, int cds )
    {
        this->set( _VM_CR_Constants, this->heap().make( cds ).cooked() );
        if ( gds )
            this->set( _VM_CR_Globals, this->heap().make( gds ).cooked() );
    }

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        MakeFrame< ConstContext > mkframe( *this, pc );
        mkframe.enter( parent, args... );
    }

    ConstContext( Program &p ) : ConstContext( p, Heap_() ) {}
    ConstContext( Program &p, const Heap_ &h ) : Base( h ), _program( &p ) {}
};

}
