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
 *   - ctx_fault needs ctx_program and ctx_ctlreg to construct the fault handler frame */

namespace divine::vm
{
    struct TraceSchedChoice { value::Pointer list; };

    using Location = _VM_Operand::Location;
    using PtrRegister = GenericPointer;
    using IntRegister = uint64_t;

    template< typename... > struct compose {};

    template< typename head, typename... tail >
    struct compose< head, tail... >
    {
        template< typename next >
        struct module : head::template module< typename compose< tail... >::template module< next > >
        {};
    };

    template<>
    struct compose<>
    {
        template< typename next >
        struct module : next {};
    };

    struct empty {};

    template< typename... comps >
    using compose_stack = typename compose< comps... >::template module< empty >;
}

namespace divine::vm::ctx
{
    template< template< typename > class mod >
    struct m
    {
        template < typename next >
        using module = mod< next >;
    };

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

        PtrRegister fault_handler() const { return _fault_handler; }
        PtrRegister state_ptr() const { return _state_ptr; }
        PtrRegister scheduler() const { return _scheduler; }

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
        virtual void trace( TraceConstraints ) {}
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

    struct ctx_choose_0
    {
        template< typename I >
        int choose( int, I, I ) { return 0; }
    };

    template< typename prog, typename heap >
    struct base
    {
        struct basement : ctx_choose_0, ctx_trace, ctx_ctlreg, ctx_program< prog > {};

        template< typename >
        struct module : ctx_heap< heap, basement >
        {
            static constexpr const bool uses_ptr2i = false;
            static constexpr const bool has_debug_mode = false;
        };
    };
}
