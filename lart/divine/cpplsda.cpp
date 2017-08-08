// -*- C++ -*- (c) 2017 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Analysis/LibCallSemantics.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/divine/cppeh.h>

namespace lart {
namespace divine {

struct AddCppLSDA {

    static PassMeta meta() {
        return passMeta< AddCppLSDA >( "AddCppLSDA",
                "Add language specific data for C++ LSDA to function's LLVM metadata" );
    }

    void run( llvm::Module &mod )
    {
        auto &ctx = mod.getContext();
        auto vptr = llvm::Type::getInt8PtrTy( ctx );
        for ( auto &fn : mod ) {
            if ( !fn.hasPersonalityFn() ||
                 llvm::classifyEHPersonality( fn.getPersonalityFn() ) != llvm::EHPersonality::GNU_CXX )
                continue;

            CppEhTab ehtab( fn );
            auto *lsda = ehtab.getLSDAConst();
            if ( !lsda )
                continue;
            auto glo = new llvm::GlobalVariable( mod, lsda->getType(), true,
                                    llvm::GlobalValue::InternalLinkage,
                                    lsda, fn.getName() + std::string( ".cpp_lsda" ) );
            auto *lsdap = llvm::ConstantExpr::getBitCast( glo, vptr );
            auto *lsdam = llvm::ConstantAsMetadata::get( lsdap );
            fn.setMetadata( "lart.lsda", llvm::MDTuple::get( ctx, { lsdam } ) );
        }
    }
};

PassMeta lsda() {
    return compositePassMeta< AddCppLSDA >( "lsda",
            "Add LSDA for exception handling." );
}
}
}

