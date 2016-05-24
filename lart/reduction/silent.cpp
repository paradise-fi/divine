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

struct SilentLocals : lart::Pass {

    static PassMeta meta() {
        return passMeta< SilentLocals >( "SilentLocals" );
    }

    template< typename I >
    auto canBeSilent( I *inst ) -> decltype( inst->getPointerOperand(), true ) {
        return !canBeCaptured( inst->getPointerOperand() );
    }

    bool canBeSilent( llvm::CallSite cs ) {
        if ( cs.getCalledValue()->stripPointerCasts() == _memcpy ) {
            return !canBeCaptured( cs.getArgument( 0 ) ) && !canBeCaptured( cs.getArgument( 1 ) );
        }
        return false;
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        util::Timer _( "SilentLocals" );

        auto *mdnode = llvm::MDNode::getDistinct( m.getContext(), { } );
        auto mdkind = getSilentKind( m );
        auto silence = [=]( llvm::Instruction *i ) { i->setMetadata( mdkind, mdnode ); };
        _memcpy = m.getFunction( "__divine_memcpy" );

        long all = 0, silenced = 0;
        query::query( m ).flatten().flatten().map( query::refToPtr )
            .forall( [&]( llvm::Instruction *i ) {
                apply( i, brick::hlist::TypeList< llvm::LoadInst, llvm::StoreInst,
                                                  llvm::AtomicCmpXchgInst,
                                                  llvm::AtomicRMWInst,
                                                  llvm::CallInst, llvm::InvokeInst >(),
                    [&]( auto i ) {
                        ++all;
                        if ( this->canBeSilent( i ) ) {
                            ++silenced;
                            silence( i );
                        }
                    } );
              } );

        std::cerr << "INFO: silenced " << silenced << " memory instructions out of " << all << std::endl;

        return llvm::PreservedAnalyses::all();
    }

  private:
    bool canBeCaptured( llvm::Value *v ) { return util::canBeCaptured( v, &_escMap ); }

    util::Map< llvm::Value *, bool > _escMap;
    llvm::Function *_memcpy;
};

PassMeta silentPass() {
    return compositePassMeta< SilentLocals >( "silent", "mark memory instructions which are known to not have effect obeservable by other threads as silent" );
}


}
}
