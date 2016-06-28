// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-string>
#include <string>
#include <iostream>
#include <cxxabi.h>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/util.h>
#include <lart/support/metadata.h>
#include <lart/support/cleanup.h>

namespace lart {
namespace divine {

struct HideSymbols : lart::Pass {

    static PassMeta meta() {
        return passMeta< HideSymbols >( "HideSymbols",
                "Make all defined symbols hidden so that they cannot be accessed by shared library." );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        query::query( m )
            .map( query::refToPtr )
            .filter( []( auto *fn ) { return !fn->empty(); } )
            .map( query::llvmcast< llvm::GlobalValue > )
            .append( query::query( m.globals() )
                        .map( query::refToPtr )
                        .filter( []( auto *g ) { return g->hasInitializer(); } )
                        .map( query::llvmcast< llvm::GlobalValue > ) )
            .append( query::query( m.aliases() )
                        .map( query::refToPtr )
                        .map( query::llvmcast< llvm::GlobalValue > ) )
            .filter( []( auto *g ) {
                    return ( g->hasExternalLinkage()
                                || g->hasExternalWeakLinkage()
                                || g->hasWeakLinkage() )
                            && !g->getName().startswith( "__md_" )
                            && !g->getName().startswith( "__vm_" )
                            && !g->getName().startswith( "__sys_" );
                } )
            .forall( []( auto *g ) {
                    g->setVisibility( llvm::GlobalValue::VisibilityTypes::HiddenVisibility );
                } );
        return llvm::PreservedAnalyses::none();
    }
};

struct NativeStart : lart::Pass {

    static PassMeta meta() {
        return passMeta< NativeStart >( "NativeStart",
                "add functions to allow starting the model natively" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        auto *voidFunT = llvm::FunctionType::get( llvm::Type::getVoidTy( m.getContext() ), false );

        // create __native_start_rt declaration (will be provided by libnativert.so
        auto *nativeStartRT = llvm::cast< llvm::Function >(
                m.getOrInsertFunction( "__native_start_rt", voidFunT ) );

        // create __native_start which invokes __native_start_rt
        // __native_start_rt cannot be directly invoked from ASM sice it is
        // dynamically loaded
        auto *nativeStart = llvm::cast< llvm::Function >(
                m.getOrInsertFunction( "__native_start", voidFunT ) );
        auto *bb = llvm::BasicBlock::Create( m.getContext(), "entry", nativeStart );
        llvm::IRBuilder<> irb( bb );
        irb.CreateCall( nativeStartRT );
        irb.CreateUnreachable();

        // insert _start as inline ASM
        m.getOrInsertFunction( "_start", voidFunT );
#if __x86_64__
        m.appendModuleInlineAsm(
                    ".text\n"
                    ".global _start\n"
                    "_start:\n"
                    "	xor %rbp,%rbp\n" // zero base pointer (expected by ABI)
                    "	andq $-16,%rsp\n" // align stack for SSE instructions (expected by codegen)
                    "	call __native_start\n"
                );
#else
#error Please provide _start for your platform
#endif
        return llvm::PreservedAnalyses::none();
    }
};

PassMeta makeNativePass() {
    return compositePassMeta< HideSymbols, NativeStart >( "makenative",
            "Turn DIVINE-compiled bitcode into natively executable one." );
}

}
}

