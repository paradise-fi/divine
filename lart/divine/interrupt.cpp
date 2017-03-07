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

#include <divine/vm/divm.h>

namespace lart {
namespace divine {

struct CflInterrupt : lart::Pass {

    static PassMeta meta() {
        return passMeta< CflInterrupt >( "CflInterrupt", "Instrument all control flow cycles "
                                         "with __vm_interrupt_cfl intrinsic." );
    }

    void annotateFn( llvm::Function &fn ) {

        for ( auto b : getBackEdges( fn ) ) {
            auto *src = b.from;
            auto idx = b.succIndex;
            auto *term = src->getTerminator();
            auto *dst = term->getSuccessor( idx );
            ASSERT_LEQ( 1, term->getNumSuccessors() );

            if ( term->getNumSuccessors() == 1 ) {
                llvm::IRBuilder<>( term ).CreateCall( _cflInterrupt, { } );
            } else if ( dst->getUniquePredecessor() ) {
                llvm::IRBuilder<>( dst->getFirstInsertionPt() ).CreateCall( _cflInterrupt, { } );
            } else {
                // neither of the blocks of the edge is used only by this path,
                // insert backedge block for savepc and fix branch and PHI nodes
                auto *back = llvm::BasicBlock::Create( fn.getParent()->getContext(), "backedge", &fn );
                llvm::IRBuilder<> irb( back );
                irb.CreateCall( _cflInterrupt, { } );
                irb.CreateBr( dst );

                // re-wire terminator and phi nodes
                term->setSuccessor( idx, back );
                for ( auto &inst : *dst ) {
                    auto *phi = llvm::dyn_cast< llvm::PHINode >( &inst );
                    if ( !phi )
                        break;
                    for ( unsigned i = 0, end = phi->getNumIncomingValues(); i < end; ++i ) {
                        if ( phi->getIncomingBlock( i ) == src )
                            phi->setIncomingBlock( i, back );
                    }
                }
            }
            ++_backedges;
        }
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        if ( !tagModuleWithMetadata( m, "lart.divine.interrupt.cfl" ) )
            return llvm::PreservedAnalyses::all();

        auto *spcty = llvm::FunctionType::get( llvm::Type::getVoidTy( m.getContext() ), false );
        _cflInterrupt = llvm::cast< llvm::Function >( m.getOrInsertFunction( "__vm_interrupt_cfl", spcty ) );
        ASSERT( _cflInterrupt );
        _cflInterrupt->addFnAttr( llvm::Attribute::NoUnwind );

        std::set< llvm::Function * > skip;
        brick::llvm::enumerateFunctionsForAnno( "lart.interrupt.skipcfl", m,
                                                [&]( llvm::Function *f ) { skip.insert( f ); } );

        for ( auto &fn : m ) {
            if ( fn.empty() || skip.count( &fn ) )
                continue;
            annotateFn( fn );
        }
        // std::cout << "Found " << _backedges << " backedges" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

    llvm::Function *_cflInterrupt;
    long _backedges = 0;
};

struct MemInterrupt : lart::Pass {

    static PassMeta meta() {
        return passMeta< MemInterrupt >( "MemInterrupt", "Instrument all memory accesses with "
                                         "__vm_interrupt_mem intrinsic." );
    }

    void annotateFn( llvm::Function &fn ) {
        // avoid changing bb while we iterate over it
        for ( auto inst : query::query( fn ).flatten().map( query::refToPtr ).freeze() ) {
            auto op = inst->getOpcode();
            if ( op == llvm::Instruction::Load || op == llvm::Instruction::Store
                || op == llvm::Instruction::AtomicRMW
                || op == llvm::Instruction::AtomicCmpXchg )
            {
                auto *type = _memInterrupt->getFunctionType();
                auto point = llvm::BasicBlock::iterator( inst );
                llvm::IRBuilder<> irb{ point };
                auto ptr = irb.CreateBitCast( getPointerOperand( inst ), type->getParamType( 0 ) );
                irb.CreateCall( _memInterrupt, { ptr, irb.getInt32( _VM_MAT_Load ) } );
                point ++;
                if ( op != llvm::Instruction::Load )
                {
                    irb.SetInsertPoint( point );
                    irb.CreateCall( _memInterrupt, { ptr, irb.getInt32( _VM_MAT_Store ) } );
                }
                ++_mem;
            }
        }
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        if ( !tagModuleWithMetadata( m, "lart.divine.interrupt.mem" ) )
            return llvm::PreservedAnalyses::all();

        auto &ctx = m.getContext();
        auto *ty = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ),
                            { llvm::Type::getInt8PtrTy( ctx ), llvm::Type::getInt32Ty( ctx ) },
                            false );
        _memInterrupt = llvm::cast< llvm::Function >( m.getOrInsertFunction( "__vm_interrupt_mem", ty ) );
        ASSERT( _memInterrupt );
        _memInterrupt->addFnAttr( llvm::Attribute::NoUnwind );

        for ( auto &fn : m ) {
            if ( fn.empty() )
                continue;
            annotateFn( fn );
        }
        // std::cout << "Found " << _mem << " memory accesses" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

    llvm::Function *_memInterrupt;
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

