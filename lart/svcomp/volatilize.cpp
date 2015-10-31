// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <brick-string.h>
#include <unordered_set>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

#include <brick-types.h>
#include <brick-llvm.h>

namespace lart {
namespace svcomp {

struct Atomic : lart::Pass {

    static PassMeta meta() {
        return passMeta< Atomic >( "atomic", "" );
    }

    void replace( llvm::Function *orig, llvm::Function *replacement ) {
        ASSERT( replacement );
        if ( !orig )
            return;

        ASSERT( orig->getFunctionType() == replacement->getFunctionType() );
        orig->replaceAllUsesWith( replacement );
        orig->eraseFromParent();
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        auto vbegin = m.getFunction( "__VERIFIER_atomic_begin" ),
             vend = m.getFunction( "__VERIFIER_atomic_end" ),
             dbegin = m.getFunction( "__divine_interrupt_mask" ),
             dend = m.getFunction( "__divine_interrupt_unmask" );

        if ( !dbegin || !dend ) {
            std::cerr << "WARN: no atomic intrinsics found, skipping pass 'atomic'" << std::endl;
            return llvm::PreservedAnalyses::all();
        }

        replace( vbegin, dbegin );
        replace( vend, dend );

        llvm::StringRef atomicPrefix( "__VERIFIER_atomic_" );

        long atomicfs = 0;

        for ( auto &f : m ) {
            if ( !f.empty() && f.getName().startswith( atomicPrefix ) ) {
                f.addFnAttr( llvm::Attribute::NoInline );
                llvm::IRBuilder<> irb( f.getEntryBlock().getFirstInsertionPt() );
                irb.CreateCall( dbegin, { } );
                ++atomicfs;
            }
        }

        std::cerr << "INFO: atomized " << atomicfs << " functions" << std::endl;

        return llvm::PreservedAnalyses::none();
    }
};

struct Volatilize : lart::Pass {

    static PassMeta meta() {
        return passMeta< Volatilize >( "Volatilize", "" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        long changed = 0;
        auto ci = brick::llvm::CompileUnitInfo( m.getFunction( "main" ) );
        if ( !ci.valid() )
            std::cerr << "WARN: not module info, making all variables volatile" << std::endl;
        auto globals = ci.valid()
            ? query::owningQuery( ci.globals() )
                .map( query::llvmdyncast< llvm::GlobalVariable > )
                .filter( query::notnull )
                .freeze()
            : query::query( m.globals() ).map( query::refToPtr ).freeze();

        query::owningQuery( globals )
            .concatMap( []( llvm::GlobalVariable *var ) { return std::vector< llvm::Value * >( var->user_begin(), var->user_end() ); } )
            .forall( [&]( llvm::Value *v ) {
                llvmcase( v,
                    [&]( llvm::LoadInst *load ) { load->setVolatile( true ); ++changed; },
                    [&]( llvm::StoreInst *store ) { store->setVolatile( true ); ++changed; },
                    [&]( llvm::MemIntrinsic *mi ) { mi->setVolatile( llvm::ConstantInt::get( llvm::Type::getInt1Ty( m.getContext() ), 1 ) ); ++changed; }
                );
            } );

        std::cout << "INFO: set volatile on " << changed << " instructions" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

};

PassMeta volatilizePass() {
    return compositePassMeta< Volatilize, Atomic >( "main-volatilize", "" );
}

}
}

