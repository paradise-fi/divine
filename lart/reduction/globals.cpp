// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

#include <lart/reduction/passes.h>
#include <lart/support/pass.h>
#include <lart/support/util.h>
#include <lart/support/query.h>

namespace lart {
namespace reduction {

struct ReadOnlyGlobals : lart::Pass {

    static PassMeta meta() {
        return passMeta< ReadOnlyGlobals >( "ReadOnlyGlobals" );
    }

    bool readOnly( llvm::GlobalValue &glo ) {
//        glo.dump();
        bool readOnly = true;
        return query::query( glo.users() ).all( []( llvm::Value *v ) { return llvm::isa< llvm::LoadInst >( v ); } );
        /*
        for ( auto use : glo.users() ) {
            llvmcase( use,
                []( llvm::LoadInst * ) { }, // this is allowed
                [&]( llvm::StoreInst *st ) { readOnly = false; },
                [&]( llvm::CallInst *call ) {
                    readOnly = false;
//                    std::cerr << "CALL: " << std::flush; call->dump();
                },
                [&]( llvm::Value *use ) {
                    readOnly = false;
//                    std::cerr << "USE: " << std::flush; use->dump();
                } );

            if ( !readOnly )
                break;
        }
        return readOnly;
        */
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        long all = 0, constified = 0;
        for ( auto &glo : m.globals() ) {
            if ( !glo.isConstant() && !glo.isExternallyInitialized() ) {
                ++all;
                if ( glo.hasUniqueInitializer() && readOnly( glo ) ) {
                    ++constified;
                    glo.setConstant( true );
                }
            }
        }

        std::cerr << "INFO: constified " << constified << " global out of " << all << std::endl;

        return llvm::PreservedAnalyses::none();
    }
};

PassMeta globalsPass() {
    return compositePassMeta< ReadOnlyGlobals >( "globals", "Optimize usage of global variables" );
}


}
}
