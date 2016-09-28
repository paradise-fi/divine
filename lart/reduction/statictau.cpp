// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/reduction/passes.h>
#include <lart/support/pass.h>
#include <lart/support/util.h>
#include <lart/support/query.h>

#include <brick-hlist>

namespace lart {
namespace reduction {

unsigned getSilentKind( llvm::Module &m ) {
    return m.getMDKindID( "lart.silent" );
}

struct SimpleEscape : lart::Pass {

    static PassMeta meta() {
        return passMeta< SimpleEscape >( "SimpleEscape",
                "simple escape analysis and __vm_interrupt_mem removeal for memory accesses within a function" );
    }

    static void silence( llvm::Instruction *i ) {

        auto fn = i->getParent()->getParent();
        std::vector< llvm::Instruction * > toDeleteExtra;
        auto toDelete = query::query( *fn ).flatten().map( query::refToPtr )
                          .map( query::llvmdyncast< llvm::CallInst > )
                          .filter( query::notnull )
                          .filter( [&]( auto *call ) {
                              ASSERT( call );
                              bool del = call->getCalledFunction()
                                  && call->getCalledFunction()->getName() == "__vm_interrupt_mem"
                                  && call->getArgOperand( 0 )->stripPointerCasts() == i;
                              if ( !del )
                                  return false;
                              auto *arg = llvm::dyn_cast< llvm::Instruction >( call->getArgOperand( 0 ) );
                              if ( arg && arg->hasOneUse() )
                                  toDeleteExtra.push_back( arg );
                              return true;
                          } )
                          .map( []( auto *i ) { ASSERT( i ); return llvm::cast< llvm::Instruction >( i ); } )
                          .freeze();

        for ( auto x : query::query( toDelete ).append( toDeleteExtra ) ) {
            x->eraseFromParent();
        }
    }

    static bool isAllocation( llvm::Instruction *i ) {
        if ( llvm::isa< llvm::AllocaInst >( i ) )
            return true;
        llvm::CallSite cs( i );
        if ( !cs || !cs.getCalledFunction() )
            return false;
        auto name = cs.getCalledFunction()->getName();
        return name == "__vm_obj_make"
               || name == "malloc" || name == "calloc" || name == "realloc"
               || name.startswith( "_Znwm" ) || name.startswith( "_Znam" );
    }

    static bool isSafe( llvm::Instruction *i, llvm::Instruction * ) {
        return false;
    }
    static bool isSafe( llvm::DbgInfoIntrinsic *, llvm::Instruction * ) { return true; }
    // ret is OK, if there are not other escapes the value cannot escape in this function
    static bool isSafe( llvm::ReturnInst *i, llvm::Instruction * ) { return true; }
    static bool isSafe( llvm::LoadInst *, llvm::Instruction * ) { return true; }
    static bool isSafe( llvm::StoreInst *i, llvm::Instruction *alloc ) {
        return i->getPointerOperand()->stripPointerCasts() == alloc;
    }
    static bool isSafe( llvm::AtomicRMWInst *i, llvm::Instruction *alloc ) {
        return i->getPointerOperand()->stripPointerCasts() == alloc;
    }
    static bool isSafe( llvm::AtomicCmpXchgInst *i, llvm::Instruction *alloc ) {
        return i->getPointerOperand()->stripPointerCasts() == alloc;
    }
    static bool isSafe( llvm::BitCastInst *i, llvm::Instruction *alloc ) {
        return i->stripPointerCasts() == alloc
            && doesNotEscape( i, alloc );
    }
    static bool isSafe( llvm::CallInst *i, llvm::Instruction *alloc ) {
        return i->getCalledFunction()
            && i->getCalledFunction()->getName() == "__vm_interrupt_mem"
            && i->getArgOperand( 0 )->stripPointerCasts() == alloc;
    }
    static bool isSafe( llvm::MemSetInst *i, llvm::Instruction *alloc ) {
        return i->getRawDest()->stripPointerCasts() == alloc;
    }

    static bool doesNotEscape( llvm::Instruction *i, llvm::Instruction *alloc = nullptr ) {
        alloc = alloc ?: i;
        return query::query( i->users() )
                .map( query::llvmdyncast< llvm::Instruction > )
                .filter( query::notnull )
                .all( [alloc]( llvm::Instruction *i ) {
                    return applyInst( i, [alloc]( auto x ) { return isSafe( x, alloc ); } );
                } );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        long all = 0, silenced = 0;
        auto allocations = query::query( m ).flatten().flatten().map( query::refToPtr )
                                            .filter( isAllocation );

        for ( auto a : allocations ) {
            ++all;
            if ( doesNotEscape( a ) ) {
                ++silenced;
                silence( a );
            }
        }

        return llvm::PreservedAnalyses::all();
    }

  private:
    bool canBeCaptured( llvm::Value *v ) { return util::canBeCaptured( v, &_escMap ); }

    util::Map< llvm::Value *, bool > _escMap;
};

PassMeta staticTauMemPass() {
    return compositePassMeta< SimpleEscape >( "statictaumem", "mark memory instructions which are known to not have effect obeservable by other threads as silent" );
}


}
}
