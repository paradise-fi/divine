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
#include <divine/vm/dbg-info.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

namespace divine::vm::dbg
{

template< typename Heap >
struct DNContext : vm::Context< Program, Heap >
{
    using Super = vm::Context< Program, Heap >;
    using Snapshot = typename Heap::Snapshot;
    using RefCnt = brick::mem::SlavePool< typename Heap::SnapPool >;

    Info *_debug;
    RefCnt _refcnt;

    Info &debug() { return *_debug; }
    DNContext( Program &p, Info &i, const Heap &h )
        : Super( p, h ), _debug( &i ), _refcnt( this->heap()._snapshots ) {}

    template< typename Ctx >
    void load( const Ctx &ctx )
    {
        Super::load( ctx );
        _refcnt = RefCnt( this->heap()._snapshots );
    }

    void load( typename Heap::Snapshot snap ) { Super::load( snap ); }
};

template< typename Heap >
struct Context : DNContext< Heap >
{
    using Super = DNContext< Heap >;
    std::vector< std::string > _trace;
    std::string _info;

    Step _lock;
    enum { LockDisabled, LockScheduler, LockChoices, LockBoth } _lock_mode;
    ProcInfo _proc;

    llvm::DIType *_state_di_type;
    llvm::Type   *_state_type;

    int _debug_depth = 0;
    uint64_t _debug_shuffle;

    Context( Program &p, dbg::Info &dbg ) : Context( p, dbg, Heap() ) {}
    Context( Program &p, dbg::Info &dbg, const Heap &h )
        : DNContext< Heap >( p, dbg, h ), _lock_mode( LockDisabled ),
          _state_di_type( nullptr ), _state_type( nullptr )
    {}

    bool enter_debug()
    {
        ASSERT_EQ( _debug_depth, 0 );
        ASSERT( !this->_debug_mode );
        _debug_shuffle = this->ref( _VM_CR_ObjIdShuffle ).integer;
        this->_debug_mode = true;
        return true;
    }

    void entered( CodePointer pc )
    {
        if ( this->_debug_mode )
            ++ _debug_depth;
        return Super::entered( pc );
    }

    void left( CodePointer pc )
    {
        if ( this->_debug_mode )
        {
            ASSERT_LEQ( 1, _debug_depth );
            -- _debug_depth;
            if ( !_debug_depth )
            {
                this->_debug_mode = false;
                this->ref( _VM_CR_ObjIdShuffle ).integer = _debug_shuffle;
            }
        }
        return Super::left( pc );
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
        switch ( _lock_mode )
        {
            case LockDisabled: case LockScheduler:
                if ( _lock.choices.empty() )
                    _lock.choices.emplace_back( 0, 0 ); break;
            default:
                ASSERT( !_lock.choices.empty() );
        }

        auto front = _lock.choices.front();
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
    void maybe_interrupt( Interrupt::Type t, CodePointer pc, Upcall up, Args... args )
    {
        if( this->in_kernel() )
            return;

        if ( _lock_mode != LockBoth )
            return (this->*up)( pc, args... );

        if ( !_lock.interrupts.empty() && this->_instruction_counter == _lock.interrupts.front().ictr )
        {
            ASSERT_EQ( t, _lock.interrupts.front().type );
            ASSERT_EQ( pc, _lock.interrupts.front().pc );
            _lock.interrupts.pop_front();
            this->set_interrupted( true );
        }
    }

    void mem_interrupt( CodePointer pc, GenericPointer ptr, int sz, int t )
    {
        maybe_interrupt( Interrupt::Mem, pc, &Super::mem_interrupt, ptr, sz, t );
    }

    void cfl_interrupt( CodePointer pc )
    {
        maybe_interrupt( Interrupt::Cfl, pc, &Super::cfl_interrupt );
    }

    void trace( vm::TraceText tt )
    {
        _trace.push_back( this->heap().read_string( tt.text ) );
    }

    void trace( vm::TraceSchedInfo ) { NOT_IMPLEMENTED(); }

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

    template < typename F >
    void find_dbg_inst( llvm::Function *f, llvm::Value *v, F yield ) {
        for ( auto &BB : *f )
            for ( auto &I : BB ) {
                if ( auto DVI = llvm::dyn_cast< llvm::DbgValueInst >( &I ) )
                        if ( DVI->getValue() == v )
                            yield( DVI->getVariable() );
                if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                    if ( DDI->getAddress() == v )
                        yield( DDI->getVariable() );
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
        auto ptrtype = llvm::cast< llvm::DIDerivedType >(
            di->getType().resolve( this->debug().typemap() ) );
        _state_di_type = ptrtype->getBaseType().resolve( this->debug().typemap() );
    }

    void trace( vm::TraceStateType s )
    {
        auto sptr = this->debug().find( nullptr, s.pc ).first->getOperand( 1 );
        _state_type = sptr->getType()->getPointerElementType();
        find_dbg_inst( sptr, [&]( llvm::DIVariable *di ) {
            state_type( di );
        } );
    }

    void trace( vm::TraceAlg ) { }

    void trace( TraceTypeAlias a ) {
        auto ptr = this->debug().find( nullptr, a.pc ).first->getOperand( 1 );
        std::string alias = this->_heap.read_string( a.alias );
        find_dbg_inst( ptr, [&]( llvm::DIVariable *di ) {
            auto ptrtype = llvm::cast< llvm::DIDerivedType >(
                di->getType().resolve( this->debug().typemap() ) );
            auto type = ptrtype->getBaseType().resolve( this->debug().typemap() );
            this->debug()._typenamemap.insert( { type, alias } );
        } );
    }
};

}
