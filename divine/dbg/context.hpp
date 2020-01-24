// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/context.hpp>
#include <divine/vm/memory.tpp>
#include <divine/dbg/info.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <random>

namespace divine::dbg
{

template< typename Heap >
struct DNContext : vm::Context< vm::Program, Heap >
{
    using Super = vm::Context< vm::Program, Heap >;
    using Snapshot = typename Heap::Snapshot;
    using SnapPool = typename Heap::SnapPool;
    using RefPool = brick::mem::RefPool< SnapPool >;
    using RefCnt  = brick::mem::RefCnt< SnapPool >;

    Info *_debug;
    SnapPool _pool;
    RefPool _refcnt;

    Info &debug() { return *_debug; }
    DNContext( vm::Program &p, Info &i, const Heap &h )
        : _debug( &i ), _refcnt( _pool )
    {
        this->program( p );
        this->heap( h );
    }

    template< typename Ctx >
    void load( SnapPool &p, const Ctx &ctx )
    {
        _pool = p;
        Super::load( ctx );
        _refcnt = RefPool( _pool );
    }

    void set_pool( SnapPool &p )
    {
        _pool = p;
        _refcnt = RefPool( _pool );
    }

    Snapshot snapshot()
    {
        auto rv = Super::snapshot( _pool );
        if constexpr ( Heap::can_snapshot() )
            if ( this->heap().is_shared( _pool, rv ) )
                _refcnt.get( rv ); /* leak :( */
        return rv;
    }

    void load( Snapshot snap ) { Super::load( _pool, snap ); }
    void load( SnapPool &p, Snapshot snap ) { Super::load( p, snap ); }
};

template< typename Heap >
struct Context : DNContext< Heap >
{
    using Super = DNContext< Heap >;
    std::vector< std::string > _trace;
    std::string _info;
    std::mt19937 _rand;

    vm::Step _lock;
    enum { LockDisabled, LockScheduler, LockChoices, LockBoth, Random } _lock_mode;
    ProcInfo _proc;

    llvm::DIType *_state_di_type;
    llvm::Type   *_state_type;

    Context( vm::Program &p, dbg::Info &dbg ) : Context( p, dbg, Heap() ) {}
    Context( vm::Program &p, dbg::Info &dbg, const Heap &h )
        : DNContext< Heap >( p, dbg, h ), _rand( std::random_device()() ),
          _lock_mode( LockDisabled ), _state_di_type( nullptr ), _state_type( nullptr )
    {
        this->enable_debug();
    }

    void reset()
    {
        DNContext< Heap >::reset();
        _info.clear();
        _trace.clear();
    }

    template< typename I >
    int choose( int count, I, I )
    {
        if ( this->debug_mode() )
        {
            trace( vm::TraceFault{ "__vm_choose is not allowed" } );
            this->fault( _VM_F_Hypercall, vm::HeapPointer(), vm::CodePointer() );
            return -1;
        }

        switch ( _lock_mode )
        {
            case LockDisabled: case LockScheduler:
                if ( _lock.choices.empty() )
                    _lock.choices.emplace_back( 0, count );
                break;
            case Random:
            {
                std::uniform_int_distribution< int > dist( 0, count - 1 );
                if ( _lock.choices.empty() )
                    _lock.choices.emplace_back( dist( _rand ), count );
            }
            default:
                ASSERT( !_lock.choices.empty() );
        }

        auto front = _lock.choices.front();
        if ( front.total )
            ASSERT_EQ( count, front.total );
        ASSERT_LT( front.taken, count );
        if ( front.total )
            ASSERT_EQ( front.total, count );
        ASSERT_LEQ( 0, front.taken );
        if ( !_proc.empty() )
            _proc.clear();
        _lock.choices.pop_front();
        return front.taken;
    }

    template< typename Upcall, typename... Args >
    bool maybe_interrupt( vm::Interrupt::Type t, vm::CodePointer pc, Upcall up, Args... args )
    {
        if ( _lock_mode != LockBoth )
            return (this->*up)( pc, args... );

        if ( !_lock.interrupts.empty() && this->instruction_count() > _lock.interrupts.front().ictr )
        {
            std::cerr << "current function: "
                      << this->debug().function( pc )->getName().str() << std::endl;
            std::cerr << "interrupt expected in:"
                      << this->debug().function( _lock.interrupts.front().pc )->getName().str()
                      << std::endl;
            std::cerr << "current counter: " << this->instruction_count() << std::endl;
            std::cerr << "expected counter: " << _lock.interrupts.front().ictr << std::endl;
            std::cerr << "expected pc: " << _lock.interrupts.front().pc << std::endl;
            UNREACHABLE( "mismatched interrupt" );
        }

        if ( !_lock.interrupts.empty() && this->instruction_count() == _lock.interrupts.front().ictr )
        {
            ASSERT_EQ( t, _lock.interrupts.front().type );
            ASSERT_EQ( pc, _lock.interrupts.front().pc );
            _lock.interrupts.pop_front();
            return true;
        }

        return false;
    }

    bool test_crit( vm::CodePointer pc, vm::GenericPointer ptr, int sz, int t )
    {
        if ( this->flags_all( _VM_CF_IgnoreCrit ) )
            return false;
        return maybe_interrupt( vm::Interrupt::Mem, pc, &Super::test_crit, ptr, sz, t );
    }

    bool test_loop( vm::CodePointer pc, int counter )
    {
        if ( this->flags_all( _VM_CF_IgnoreLoop ) )
            return false;
        return maybe_interrupt( vm::Interrupt::Cfl, pc, &Super::test_loop, counter );
    }

    using Super::trace;

    void trace( std::string s ) { _trace.push_back( s ); }

    void trace( vm::TraceSchedChoice tsc )
    {
        auto ptr = tsc.list;
        int size = this->heap().size( ptr.cooked() );
        if ( size % 12 )
            return; /* invalid */
        for ( int i = 0; i < size / 12; ++i )
        {
            vm::value::Int< 32, false > pid, tid, choice;
            this->heap().read_shift( ptr, pid );
            this->heap().read_shift( ptr, tid );
            this->heap().read_shift( ptr, choice );
            _proc.emplace_back( std::make_pair( pid.cooked(), tid.cooked() ), choice.cooked() );
        }
    }

    void trace( vm::TraceInfo ti )
    {
        _info += this->heap().read_string( ti.text ) + "\n";
    }

    bool is_same( llvm::Value *a, llvm::Value *b )
    {
        if ( !a || !b )
            return a == b;

        if ( auto bc = llvm::dyn_cast< llvm::BitCastInst >( a ) )
            if ( is_same( bc->getOperand( 0 ), b ) )
                return true;

        if ( auto bc = llvm::dyn_cast< llvm::BitCastInst >( b ) )
            if ( is_same( a, bc->getOperand( 0 ) ) )
                return true;

        return a == b;
    }

    template < typename F >
    void find_dbg_inst( llvm::Function *f, llvm::Value *v, F yield )
    {
        for ( auto &BB : *f )
            for ( auto &I : BB ) {
                if ( auto DVI = llvm::dyn_cast< llvm::DbgValueInst >( &I ) )
                        if ( is_same( DVI->getValue(), v ) )
                            yield( DVI->getValue(), DVI->getVariable() );
                if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                    if ( is_same( DDI->getAddress(), v ) )
                        yield( DDI->getAddress(), DDI->getVariable() );
            }
    }

    template < typename F >
    void find_dbg_inst( llvm::Value *val, F yield )
    {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            find_dbg_inst( inst->getParent()->getParent(), val, yield );
        else if ( auto inst = llvm::dyn_cast< llvm::Argument >( val ) )
            find_dbg_inst( inst->getParent(), val, yield );
        else
            UNREACHABLE( "dbg::Context::find_dbg_inst() failed" );
    }

    void state_type( llvm::DIVariable *di )
    {
        auto t_var = llvm::cast< llvm::DIDerivedType >( di->getType().resolve() );
        _state_di_type = t_var->getBaseType().resolve();
    }

    void trace( vm::TraceStateType s )
    {
        auto sptr = this->debug().find( nullptr, s.pc ).first->getOperand( 1 );
        _state_type = sptr->getType()->getPointerElementType();
        find_dbg_inst( sptr, [&]( auto val, llvm::DIVariable *di ) {
            state_type( di );
            _state_type = val->getType()->getPointerElementType();
        } );
    }

    void trace( vm::TraceTypeAlias a )
    {
        auto ptr = this->debug().find( nullptr, a.pc ).first->getOperand( 1 );
        std::string alias = this->_heap.read_string( a.alias );
        find_dbg_inst( ptr, [&]( auto, llvm::DIVariable *di ) {
            auto ptrtype = llvm::cast< llvm::DIDerivedType >( di->getType().resolve() );
            auto type = ptrtype->getBaseType().resolve();
            this->debug()._typenamemap.insert( { type, alias } );
        } );
    }

    bool in_component( Components comp ) /* true if any match */
    {
        if ( ( comp & component::kernel ) && this->in_kernel() )
            return true;
        return this->debug().in_component( this->pc(), comp );
    }
};

}
