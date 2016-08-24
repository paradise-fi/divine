// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>

namespace lart {
namespace divine {

struct LowerResume : lart::Pass {

    static PassMeta meta() {
        return passMeta< LowerResume >( "LowerResume", "Lower resume instruction to _Unwind_Resume." );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        auto *resumeFn = m.getFunction( "_Unwind_Resume" );
        if ( !resumeFn )
            return llvm::PreservedAnalyses::all();
        auto *resumeFnT = resumeFn->getFunctionType();
        ASSERT_EQ( resumeFnT->getNumParams(), 1 );
        auto *exceptT = resumeFnT->getParamType( 0 );

        for ( auto &fn : m ) {
            for ( auto *r : query::query( fn ).flatten()
                                .map( query::refToPtr )
                                .map( query::llvmdyncast< llvm::ResumeInst > )
                                .filter( query::notnull ).freeze() )
            {
                llvm::IRBuilder<> irb( r );
                auto *ehtuple = r->getValue();
                auto *except = irb.CreateExtractValue( ehtuple, 0 );
                ASSERT( llvm::isa< llvm::PointerType >( except->getType() ) );
                auto *call = irb.CreateCall( resumeFn,
                                { irb.CreatePointerCast( except, exceptT, "exception" ) } );
                r->replaceAllUsesWith( call );
                irb.CreateUnreachable();
                r->eraseFromParent();
            }
        }
        return llvm::PreservedAnalyses::none();
    }

};

PassMeta lowerExceptionHandling() {
    return compositePassMeta< LowerResume >( "lowereh",
            "Lower exception handling intrinsics for the use in DIVINE." );
}

}
}
