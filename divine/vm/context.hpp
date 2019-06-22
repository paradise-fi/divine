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

#include <divine/vm/loops.hpp>
#include <divine/vm/frame.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/memory.hpp>
#include <divine/vm/types.hpp>
#include <divine/vm/divm.h>

#include <brick-data>
#include <brick-types>

#include <unordered_set>

namespace llvm { class Value; }

/* The context of a computation, takes care of the following aspects:
 *  - storing and interacting with control registers (ctx_ctlreg)
 *  - interaction with the heap (ctx_heap)
 *  - debug mode and tracing (ctx_trace, ctx_debug)
 *  - taking and restoring snapshots of the computation (ctx_snapshot)
 *  - hold on to the program -- harvard architecture (ctx_program)
 *  - keeping track of faults (ctx_fault)
 *  - cache the internal pointers of 'important' objects (ctx_ptr2i)
 *
 * There are some dependencies:
 *   - ctx_ctlreg, ctx_trace and ctx_program have no further dependencies
 *   - ctx_heap needs to interact with the active frame and hence needs ctx_ctlreg
 *   - ctx_ptr2i needs registers and heap
 *   - ctx_debug needs to look at control flags and snapshot/rollback the heap: ctx_ctlreg & ctx_heap
 *     plus it needs to invalidate the ptr2i cache
 *   - ctx_snapshot needs to invalidate the ptr2i cache
 *   - ctx_fault needs to ctx_program and ctx_ctlreg to construct the fault handler frame */

namespace divine::vm
{
    struct TraceSchedChoice { value::Pointer list; };

    using Location = _VM_Operand::Location;
    using PtrRegister = GenericPointer;
    using IntRegister = uint64_t;

    template< typename... Ts >
    struct ctx_mod : Ts... {};

    /* state of a computation */
    struct State
    {
        std::array< PtrRegister, _VM_CR_PtrCount > ptr;
        std::array< PtrRegister, _VM_CR_UserCount > user;
        IntRegister flags = 0, objid_shuffle = 0;
        uint32_t instruction_counter = 0;
    };

    struct ctx_ctlreg
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
            return const_cast< ctx_ctlreg * >( this )->_ref_ptr( r );
        }

        void set( _VM_ControlRegister r, GenericPointer v )
        {
            _ref_ptr( r ) = v;
        }

        PtrRegister get_ptr( _VM_ControlRegister r ) const
        {
            return _ref_ptr( r );
        }

        IntRegister get_int( _VM_ControlRegister r ) const
        {
            if ( r == _VM_CR_Flags )
                return _state.flags;
            if ( r == _VM_CR_ObjIdShuffle )
                return _state.objid_shuffle;
            __builtin_unreachable();
        }

        uint32_t instruction_count() { return _state.instruction_counter; }
        void instruction_count( uint32_t v ) { _state.instruction_counter = v; }

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

    template< typename heap_t, typename next >
    struct ctx_heap : next
    {
        using Heap = heap_t;
        heap_t _heap;

        heap_t &heap() { return _heap; }
        const heap_t &heap() const { return _heap; }

        void heap( const heap_t &h )
        {
            _heap = h;
            if ( !this->frame().null() )
                load_pc();
        }

        auto sync_pc()
        {
            if ( this->frame().null() )
                return typename heap_t::Internal();
            auto loc = this->heap().loc( this->frame(), ptr2i( Location( _VM_CR_Frame ) ) );
            return this->heap().write( loc, PointerV( this->pc() ) );
        }

        void load_pc()
        {
            PointerV pc;
            this->heap().read( this->frame(), pc );
            this->pc( pc.cooked() );
        }

        typename Heap::Internal ptr2i( Location l )
        {
            return this->heap().ptr2i( this->get_ptr( _VM_ControlRegister( l ) ) );
        }

        void ptr2i( Location, typename Heap::Internal ) {}
        void flush_ptr2i() {}
    };

    struct ctx_trace
    {
        bool enter_debug() { return false; }
        void leave_debug() {}
        bool debug_allowed() { return false; }
        bool debug_mode() { return false; }

        /* an interface for debug mode implementation */
        void debug_save() {}
        void debug_restore() {}

        void entered( CodePointer ) {}
        void left( CodePointer ) {}

        virtual std::string fault_str() { return "(no info)"; }
        virtual void fault_clear() {}
        virtual void doublefault() {}
        virtual void fault( Fault, HeapPointer, CodePointer ) {}
        virtual void trace( TraceText ) {}
        virtual void trace( TraceFault ) {}
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
        virtual ~ctx_trace() {}
    };

    template< typename program_t >
    struct ctx_program
    {
        using Program = program_t;
        Program *_program;
        Program &program() { return *_program; }
        void program( Program &p ) { _program = &p; }
    };

    template< typename next >
    struct ctx_choose_0 : next
    {
        template< typename I >
        int choose( int, I, I ) { return 0; }
    };

    template< typename heap, typename program >
    struct ctx_base
    {
        template< typename >
        struct module : ctx_heap< heap, ctx_mod< ctx_trace, ctx_ctlreg, ctx_program< program > > >
        {
            static constexpr const bool uses_ptr2i = false;
            static constexpr const bool has_debug_mode = false;
        };
    };

    template< typename next >
    struct track_nothing : next
    {
        static_assert( !next::has_debug_mode );

        bool test_loop( CodePointer, int )
        {
            return !this->flags_all( _VM_CF_IgnoreLoop );
        }

        bool test_crit( CodePointer, GenericPointer, int, int )
        {
            return !this->flags_all( _VM_CF_IgnoreCrit );
        }

        bool track_test( Interrupt::Type, CodePointer ) { return true; }
        void reset_interrupted() {}
    };

    /* Implementation of debug mode. Include in the context stack when you want
     * dbg.call to be actually performed. Not including ctx_debug in the context
     * saves considerable resources (especially computation time). */

    template< typename next >
    struct ctx_debug : next
    {
        State _debug_state;
        PtrRegister _debug_pc;

        int _debug_depth = 0;
        bool _debug_allowed = false;
        std::vector< Interrupt > _interrupts;

        static constexpr const bool uses_ptr2i = true;
        static constexpr const bool has_debug_mode = true;

        bool track_test( Interrupt::Type t, CodePointer pc )
        {
            _interrupts.push_back( Interrupt{ t, this->_state.instruction_counter, pc } );
            return true;
        }

        bool debug_allowed() { return _debug_allowed; }
        bool debug_mode() { return this->flags_any( _VM_CF_DebugMode ); }
        void enable_debug() { _debug_allowed = true; }

        bool test_loop( CodePointer p, int i )
        {
            return !debug_mode() && next::test_loop( p, i );
        }

        bool test_crit( CodePointer p, GenericPointer a, int s, int t )
        {
            return !debug_mode() && next::test_crit( p, a, s, t );
        }

        bool enter_debug();
        void leave_debug();

        void debug_save();
        void debug_restore();

        using next::set;

        void set( _VM_ControlRegister r, uint64_t v )
        {
            next::set( r, v );
            if ( _debug_depth )
                ASSERT( debug_mode() );
        }

        void entered( CodePointer f )
        {
            if ( debug_mode() )
                ++ _debug_depth;
            else
                next::entered( f );
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
                next::left( f );
        }

        std::vector< HeapPointer > _debug_persist;
        typename next::Heap::Pool _debug_pool;
        typename next::Heap::Snapshot _debug_snap;

        using next::trace;

        virtual void trace( TraceDebugPersist t )
        {
            if ( t.ptr.type() == PointerType::Weak )
                _debug_persist.push_back( t.ptr );
            else
                {
                    this->trace( "FAULT: cannot persist a non-weak object " +
                                 brick::string::fmt( t.ptr ) );
                    this->fault( _VM_F_Memory, this->frame(), this->pc() );
                }
        }
    };

    template< typename next >
    struct ctx_ptr2i : next
    {
        using Heap = typename next::Heap;
        using HeapInternal = typename Heap::Internal;
        using Snapshot = typename Heap::Snapshot;

        static_assert( !next::uses_ptr2i );
        std::array< HeapInternal, _VM_CR_PtrCount > _ptr2i;

        using next::set;

        void set( _VM_ControlRegister r, GenericPointer v )
        {
            if ( r < _VM_CR_PtrCount )
                _ptr2i[ r ] = v.null() ? HeapInternal() : this->heap().ptr2i( v );
            next::set( r, v );
        }

        HeapInternal ptr2i( Location l )
        {
            ASSERT_LT( l, _VM_Operand::Invalid );
            ASSERT_EQ( next::ptr2i( l ), _ptr2i[ l ] );
            return _ptr2i[ l ];
        }

        void ptr2i( Location l, HeapInternal i )
        {
            if ( i )
                _ptr2i[ l ] = i;
            else
                flush_ptr2i();
        }

        void flush_ptr2i()
        {
            for ( int i = 0; i < _VM_CR_PtrCount; ++i )
                _ptr2i[ i ] = next::ptr2i( Location( i ) );
        }

        auto sync_pc()
        {
            auto newfr = next::sync_pc();
            ptr2i( Location( _VM_CR_Frame ), newfr );
            return newfr;
        }

        void load_pc()
        {
            PointerV pc;
            auto loc = this->heap().loc( this->frame(), ptr2i( Location( _VM_CR_Frame ) ) );
            this->heap().read( loc, pc );
            this->_pc = pc.cooked();
        }
    };

    template< typename next >
    struct ctx_snapshot : next
    {
        using Heap = typename next::Heap;
        using Snapshot = typename Heap::Snapshot;

        static constexpr const bool uses_ptr2i = true;

        Snapshot snapshot( typename Heap::Pool &p )
        {
            this->sync_pc();
            auto rv = this->heap().snapshot( p );
            this->flush_ptr2i();
            return rv;
        }

        using next::trace;
        virtual void trace( TraceText tt ) { trace( this->heap().read_string( tt.text ) ); }
    };

    template< typename next >
    struct ctx_fault : next
    {
        std::string _fault;

        using next::trace;
        void trace( TraceFault tt ) override { _fault = tt.string; }

        void fault_clear() override { _fault.clear(); }
        std::string fault_str() override { return _fault; }

        void doublefault() override
        {
            this->flags_set( 0, _VM_CF_Error | _VM_CF_Stop );
        }

        void fault( Fault f, HeapPointer frame, CodePointer pc ) override
        {
            auto fh = this->fault_handler();
            if ( this->debug_mode() )
            {
                this->trace( "W: " + this->fault_str() + " in debug mode (abandoned)" );
                this->fault_clear();
                this->_debug_depth = 0; /* short-circuit */
                this->leave_debug();
            }
            else if ( fh.null() )
            {
                this->trace( "FATAL: no fault handler installed" );
                doublefault();
            }
            else
                make_frame( *this, fh, PointerV( this->frame() ),
                            value::Int< 32 >( f ), PointerV( frame ), PointerV( pc ) );
        }
    };

    template< typename next >
    struct ctx_legacy : next
    {
        using Program = typename next::Program;
        using Heap = typename next::Heap;
        using Pool = typename Heap::Pool;
        using PointerV = value::Pointer;
        using Location = typename Program::Slot::Location;
        using Snapshot = typename Heap::Snapshot;
        using next::_heap;

        static constexpr const bool uses_ptr2i = true;

        using next::trace;
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

        virtual void clear()
        {
            this->_fault.clear();
            this->_interrupts.clear();
            this->reset_interrupted();
            this->flush_ptr2i();
            this->set( _VM_CR_User1, GenericPointer() );
            this->set( _VM_CR_User2, GenericPointer() );
            this->set( _VM_CR_User3, GenericPointer() );
            this->set( _VM_CR_User4, GenericPointer() );
            this->set( _VM_CR_ObjIdShuffle, 0 );
            this->_state.instruction_counter = 0;
        }

        void load( Pool &p, Snapshot snap ) { _heap.restore( p, snap ); clear(); }
        void reset() { _heap.reset(); clear(); }

        HeapPointer frame() { return next::frame(); }
        HeapPointer globals() { return next::globals(); }
        HeapPointer constants() { return next::constants(); }

        bool test_loop( CodePointer pc, int ctr )
        {
            if ( this->flags_all( _VM_CF_IgnoreLoop ) || this->debug_mode() )
                return false;

            if ( next::test_loop( pc, ctr ) )
                return this->track_test( Interrupt::Cfl, pc );
            else
                return false;
        }

        void count_instruction()
        {
            if ( !this->debug_mode() )
                ++ this->_state.instruction_counter;
        }
    };

    template< template< typename > class mod >
    struct m
    {
        template < typename next >
        using module = mod< next >;
    };

    template< typename... > struct compose {};

    template< typename head, typename... tail >
    struct compose< head, tail... > : head::template module< compose< tail... > >
    {
    };

    template<>
    struct compose<> {};

    template< typename Program, typename Heap >
    struct Context : compose< m< ctx_legacy >, m< ctx_fault >, m< ctx_debug >,
                              m< track_loops >, m< track_nothing >, m< ctx_choose_0 >,
                              m< ctx_snapshot >, m< ctx_ptr2i >, ctx_base< Heap, Program > > {};

    template< typename program_t, typename heap >
    struct ctx_const : compose< m< ctx_choose_0 >, m< track_nothing >, ctx_base< heap, program_t > >
    {
        void setup( int gds, int cds )
        {
            this->set( _VM_CR_Constants, this->heap().make( cds ).cooked() );
            if ( gds )
                this->set( _VM_CR_Globals, this->heap().make( gds ).cooked() );
        }
    };

}
