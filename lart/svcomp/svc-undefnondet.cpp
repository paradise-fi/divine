// -*- C++ -*- (c) 2018 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>

namespace lart {
namespace svcomp {

struct UndefNondet {

    static PassMeta meta() { return passMeta< UndefNondet >( "undef-nondet", "" ); }

    void run( llvm::Module &m )
    {
        auto &ctx = m.getContext();

        for ( auto &fn : m.functions() ) {
            if ( fn.getName().startswith( "__sym_val_" )
                 || fn.getName().startswith( "__VERIFIER_nondet_" ) )
            {
                fn.deleteBody();
                auto *retType = fn.getReturnType();
                auto *bb = llvm::BasicBlock::Create( ctx, "", &fn );
                llvm::IRBuilder<> irb( bb );
                auto *alloca = irb.CreateAlloca( retType );
                irb.CreateRet( irb.CreateLoad( alloca ) );
            }
        }
    }
};

PassMeta svcUndefNondetPass() {
    return compositePassMeta< UndefNondet >( "svc-undef-nondet", "" );
}

}
}
