// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <brick-string.h>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

namespace lart {
namespace divine {

struct DropEmptyDecls : lart::Pass {

    static PassMeta meta() {
        return passMeta< DropEmptyDecls >( "DropEmptyDecls", "Remove unused function declarations." );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        std::vector< llvm::Function * > toDrop;
        long all = 0;
        for ( auto &fn : m ) {
            if ( fn.isDeclaration() ) {
                ++all;
                if ( fn.user_empty() )
                    toDrop.push_back( &fn );
            }
        }
        for ( auto f : toDrop )
            f->eraseFromParent();
        std::cerr << "INFO: erased " << toDrop.size() << " empty declarations out of " << all << std::endl;
        return llvm::PreservedAnalyses::none();
    }
};

struct TrapDecls : lart::Pass {

    static PassMeta meta() {
        return passMeta< TrapDecls >( "TrapDecls", "Replace non-intrinsic function declarations with definitions containing unreachable instruction" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        auto &ctx = m.getContext();
        long all = 0;
        for ( auto &fn : m ) {
            if ( fn.isDeclaration() && !fn.isIntrinsic() && !fn.getName().startswith( "__divine_" ) ) {
                ++all;
                auto bb = llvm::BasicBlock::Create( ctx, "", &fn );
                new llvm::UnreachableInst( ctx, bb );
            }
        }
        std::cerr << "INFO: added unreachable to " << all << " declarations" << std::endl;
        return llvm::PreservedAnalyses::none();
    }
};

PassMeta declsPass() {
    return compositePassMeta< DropEmptyDecls, TrapDecls >( "decls", "Remove unused function declarations and add unreachable instruction to remaining non-intrinsic declarations" );
}

}
}
