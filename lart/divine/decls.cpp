// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <brick-string.h>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

namespace lart {
namespace divine {

enum class Problem {
    #define PROBLEM(x) x,
    #include <divine/llvm/problem.def>
    #undef PROBLEM
};

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
        return passMeta< TrapDecls >( "TrapDecls", "Replace non-intrinsic function declarations with definitions call to __divine_problem( Other, \"undefined\" )" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        auto &ctx = m.getContext();
        auto problem = m.getFunction( "__divine_problem" );
        ASSERT( problem );
        auto ptype = problem->getFunctionType()->getParamType( 0 );
        auto pstrty = problem->getFunctionType()->getParamType( 1 );

        const auto undefstr = "undefined";
        auto undefInit = llvm::ConstantDataArray::getString( ctx, undefstr );
        auto undef = llvm::cast< llvm::GlobalVariable >(
                m.getOrInsertGlobal( "lart.divine.decls.undefined.str", undefInit->getType() ) );
        undef->setConstant( true );
        undef->setInitializer( undefInit );

        long all = 0;
        for ( auto &fn : m ) {
            if ( fn.isDeclaration() && !fn.isIntrinsic() && !fn.getName().startswith( "__divine_" ) ) {
                ++all;
                auto bb = llvm::BasicBlock::Create( ctx, "", &fn );
                llvm::IRBuilder<> irb( bb );
                auto undefPt = irb.CreateBitCast( undef, pstrty );
                irb.CreateCall( problem, { llvm::ConstantInt::get( ptype, int( Problem::Other ) ), undefPt } );
                if ( fn.getReturnType()->isVoidTy() )
                    irb.CreateRetVoid();
                else
                    irb.CreateRet( llvm::UndefValue::get( fn.getReturnType() ) );
            }
        }
        std::cerr << "INFO: added unreachable to " << all << " declarations" << std::endl;
        return llvm::PreservedAnalyses::none();
    }
};

PassMeta declsPass() {
    return compositePassMeta< DropEmptyDecls, TrapDecls >( "decls", "Remove unused function declarations and add __divine_problem to remaining non-intrinsic declarations" );
}

}
}
