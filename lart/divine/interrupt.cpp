// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Dominators.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-string>
#include <brick-llvm>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/support/metadata.h>
#include <lart/divine/passes.h>
#include <lart/reduction/passes.h>

#include <divine/vm/divm.h>

namespace lart {
namespace divine {

struct CflInterrupt
{
    static PassMeta meta()
    {
        return passMeta< CflInterrupt >( "CflInterrupt", "Annotate control flow cycles." );
    }

    void insert( llvm::Value *v, llvm::IRBuilder<> &b ) { b.CreateCall( _hypercall, { v, _handler } ); }
    void insert( llvm::Value *v, llvm::Instruction *where )
    {
        llvm::IRBuilder<> b( where );
        insert( v, b );
    }

    void annotateFn( llvm::Function &fn )
    {
        for ( auto b : getBackEdges( fn ) )
        {
            auto *src = b.from;
            auto idx = b.succIndex;
            auto *term = src->getTerminator();
            auto *dst = term->getSuccessor( idx );
            ASSERT_LEQ( 1, term->getNumSuccessors() );

            auto i32_t = llvm::Type::getInt32Ty( fn.getParent()->getContext() );
            auto ctr = llvm::ConstantInt::get( i32_t, 0 );

            // Try to be smart and avoid invoking unnecessary interrupts which
            // are not on back edges.
            // However, for IndirectBr, there is no way to be smart as the
            // successors in the code are only hints for LLVM and real
            // successors are determined at runtime and therefore cannot be
            // rewritten -- so for IndirectBr we fallback to instrumenting
            // eagerly.
            if ( term->getNumSuccessors() == 1 ||
                 term->getOpcode() == llvm::Instruction::IndirectBr )
                insert( ctr, term );
            else if ( dst->getUniquePredecessor() )
                insert( ctr, dst->getFirstInsertionPt() );
            else
            {
                // neither of the blocks of the edge is used only by this path,
                // insert backedge block for savepc and fix branch and PHI nodes
                auto *back = llvm::BasicBlock::Create( fn.getParent()->getContext(), "backedge", &fn );
                llvm::IRBuilder<> irb( back );
                insert( ctr, irb );
                irb.CreateBr( dst );

                // re-wire terminator and phi nodes
                term->setSuccessor( idx, back );
                for ( auto &inst : *dst )
                {
                    auto *phi = llvm::dyn_cast< llvm::PHINode >( &inst );
                    if ( !phi )
                        break;

                    for ( unsigned i = 0, end = phi->getNumIncomingValues(); i < end; ++i )
                        if ( phi->getIncomingBlock( i ) == src )
                            phi->setIncomingBlock( i, back );
                }
            }
            ++_backedges;
        }
    }

    void run( llvm::Module &m )
    {
        if ( !tagModuleWithMetadata( m, "lart.divine.interrupt.cfl" ) )
            return;

        auto void_t = llvm::Type::getVoidTy( m.getContext() );
        auto i32_t = llvm::Type::getInt32Ty( m.getContext() );
        auto handler_t = llvm::FunctionType::get( void_t, false );
        auto hyper_t = llvm::FunctionType::get( void_t, { i32_t, handler_t->getPointerTo() }, false );

        auto hyper = m.getOrInsertFunction( "__vm_test_loop", hyper_t );
        auto handler = m.getOrInsertFunction( "__dios_interrupt", handler_t );

        _hypercall = llvm::cast< llvm::Function >( hyper );
        _handler = llvm::cast< llvm::Function >( handler );

        ASSERT( _hypercall );
        ASSERT( _handler );

        _hypercall->addFnAttr( llvm::Attribute::NoUnwind );
        _handler->addFnAttr( llvm::Attribute::NoUnwind );

        std::set< llvm::Function * > skip;
        brick::llvm::enumerateFunctionsForAnno( "lart.interrupt.skipcfl", m,
                                                [&]( llvm::Function *f ) { skip.insert( f ); } );

        for ( auto &fn : m )
        {
            if ( fn.empty() || skip.count( &fn ) )
                continue;
            annotateFn( fn );
        }
    }

    llvm::Function *_hypercall, *_handler;
    long _backedges = 0;
};

struct MemInterrupt
{
    static PassMeta meta()
    {
        return passMeta< MemInterrupt >( "MemInterrupt", "Annotate (visible) memory accesses." );
    }

    void annotateFn( llvm::Function &fn, llvm::DataLayout &dl, unsigned silentID )
    {
        // avoid changing bb while we iterate over it
        for ( auto inst : query::query( fn ).flatten().map( query::refToPtr ).freeze() )
        {
            auto op = inst->getOpcode();
            if ( ( op == llvm::Instruction::Load || op == llvm::Instruction::Store ||
                   op == llvm::Instruction::AtomicRMW ||
                   op == llvm::Instruction::AtomicCmpXchg ) &&
                 !reduction::isSilent( *inst, silentID ) )
            {
                auto *type = _hypercall->getFunctionType();
                auto point = llvm::BasicBlock::iterator( inst );
                llvm::IRBuilder<> irb{ point };
                auto *origPtr = getPointerOperand( inst );
                auto *origT = llvm::cast< llvm::PointerType >( origPtr->getType() )
                                  ->getElementType();
                auto *ptr = irb.CreateBitCast( origPtr, type->getParamType( 0 ) );
                auto *si = irb.getInt32( std::max( uint64_t( 1 ), dl.getTypeSizeInBits( origT ) / 8 ) );
                point ++;
                int intr_type;
                switch ( op )
                {
                    case llvm::Instruction::Load: intr_type = _VM_MAT_Load; break;
                    case llvm::Instruction::Store: intr_type = _VM_MAT_Store; break;
                    default: intr_type = _VM_MAT_Both; break;
                }
                irb.SetInsertPoint( point );
                irb.CreateCall( _hypercall, { ptr, si, irb.getInt32( intr_type ), _handler } );
                ++_mem;
            }
        }
    }

    void run( llvm::Module &m )
    {
        if ( !tagModuleWithMetadata( m, "lart.divine.interrupt.mem" ) )
            return;

        auto i32_t = llvm::Type::getInt32Ty( m.getContext() );
        auto void_t = llvm::Type::getVoidTy( m.getContext() );
        auto i8_p = llvm::Type::getInt8PtrTy( m.getContext() );
        auto handler_t = llvm::FunctionType::get( void_t, false );
        auto hptr_t = handler_t->getPointerTo();
        auto hyper_t = llvm::FunctionType::get( void_t, { i8_p, i32_t, i32_t, hptr_t }, false );

        auto hyper = m.getOrInsertFunction( "__vm_test_crit", hyper_t );
        auto handler = m.getOrInsertFunction( "__dios_interrupt", handler_t );

        _hypercall = llvm::cast< llvm::Function >( hyper );
        _handler = llvm::cast< llvm::Function >( handler );

        ASSERT( _hypercall );
        ASSERT( _handler );

        _hypercall->addFnAttr( llvm::Attribute::NoUnwind );
        _handler->addFnAttr( llvm::Attribute::NoUnwind );

        auto silentID = m.getMDKindID( reduction::silentTag );
        llvm::DataLayout dl( &m );

        std::set< llvm::Function * > skip;
        brick::llvm::enumerateFunctionsForAnno( "lart.interrupt.skipmem", m,
                                                [&]( llvm::Function *f ) { skip.insert( f ); } );

        for ( auto &fn : m )
        {
            if ( fn.empty() || skip.count( &fn ) )
                continue;
            annotateFn( fn, dl, silentID );
        }
    }

    llvm::Function *_hypercall, *_handler;
    long _mem = 0;
};

PassMeta interruptPass()
{
    return compositePassMeta< CflInterrupt, MemInterrupt >(
        "interrupt",
        "Instrument bitcode for DIVINE: add __vm_interrupt_mem and "
        "__vm_interrupt_cfl calls to appropriate locations." );
}

PassMeta cflInterruptPass()
{
    return passMeta< CflInterrupt >(
        "interrupt-cfl", "add __vm_interrupt_cfl calls to appropriate locations." );
}

}
}

