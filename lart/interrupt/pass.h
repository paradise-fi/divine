// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/CallSite.h>

#include <brick-assert.h>

#include <iostream>
#include <unordered_set>

namespace lart {
namespace interrupt {

struct EliminateInterrupt : llvm::ModulePass {

    static char ID;

    EliminateInterrupt() : llvm::ModulePass( ID ) {}

    bool runOnModule( llvm::Module &m ) {
        auto interrupt = m.getFunction( "__divine_interrupt" );
        std::vector< llvm::CallInst * > interrupts;
        for ( auto &fn : m )
            for ( auto &bb : fn )
                for ( auto &i : bb )
                    if ( auto call = llvm::dyn_cast< llvm::CallInst >( &i ) )
                        if ( call->getCalledFunction() == interrupt )
                            interrupts.push_back( call );
        for ( auto call : interrupts )
            call->eraseFromParent();
        std::cout << "INFO: erased " << interrupts.size() << " interrupts" << std::endl;
        return true;
    }
};

struct HoistMasks : llvm::ModulePass {

    static char ID;

    HoistMasks() : llvm::ModulePass( ID ), _total( 0 ), _hoisted( 0 ) {}

    // returns true if instruction can cause data or control escape, or is load
    bool canEscape( llvm::Instruction *i, std::unordered_set< llvm::Value * > &allocas ) {
        if ( auto alloca = llvm::dyn_cast< llvm::AllocaInst >( i ) ) {
            allocas.insert( alloca );
            return false;
        }
        if ( auto store = llvm::dyn_cast< llvm::StoreInst >( i ) )
            return allocas.count( store->getPointerOperand() ) == 0;
        if ( auto load = llvm::dyn_cast< llvm::LoadInst >( i ) )
            return allocas.count( load->getPointerOperand() ) == 0;
        using I = llvm::Instruction;
        auto op = i->getOpcode();
        return op != I::Add && op != I::FAdd && op != I::Sub && op != I::FSub
            && op != I::Mul && op != I::FMul
            && op != I::UDiv && op != I::SDiv && op != I::FDiv
            && op != I::URem && op != I::SRem && op != I::FRem
            && !i->isShift()
            && op != I::And && op != I::Or && op != I::Xor
            && !i->isCast()
            && op != I::ICmp && op != I::FCmp
            && !llvm::isa< llvm::DbgInfoIntrinsic >( i );
    }

    void transform( llvm::BasicBlock &bb ) {
        llvm::Instruction *insertPoint = nullptr;
        bool moving = false;
        std::unordered_set< llvm::Value * > allocas;

        for ( auto it = bb.begin(); it != bb.end(); ++it ) {
            llvm::Instruction *i = &*it;
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
                if ( call->getCalledFunction() == _mask ) {
                    ++_total;
                    if ( moving ) {
                        ++_hoisted;
                        call->removeFromParent();
                        if ( insertPoint )
                            call->insertAfter( insertPoint );
                        else
                            call->insertBefore( bb.begin() );

                        // we must run transformation on this basic block from
                        // the point we moved mask into
                        auto it2 = bb.begin();
                        for ( ; &*it2 != i; ++it2 ) { }
                        it = it2;
                        // no need to set insertPoint, escape detection will grab
                        // this insertruction anyway
                    }
                }
            if ( canEscape( i, allocas ) ) {
                insertPoint = i;
                moving = false;
                allocas.clear();
            } else
                moving = true;
        }
    }

    bool runOnModule( llvm::Module &m ) {
        _mask = m.getFunction( "__divine_interrupt_mask" );
        if ( !_mask ) {
            std::cerr << "ERROR: could not find __divine_interrupt_mask" << std::endl;
            return false;
        }

        for ( auto &fn : m )
            for ( auto &bb : fn )
                transform( bb );

        std::cout << "INFO: hoisted " << _hoisted << " out of " << _total
                  << " interrupt masks (" << 100 * double( _hoisted ) / _total << " %)"  << std::endl;
        return true;
    }

  private:
    llvm::Function *_mask;
    int _total, _hoisted;
};

}
}
