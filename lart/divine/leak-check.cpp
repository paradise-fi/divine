// -*- C++ -*- (c) 2018 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <divine/vm/divm.h>

namespace lart::divine {

struct LeakCheck {

    static PassMeta meta() {
        return passMeta< LeakCheck >( "LeakCheck", "" );
    }

    void run( llvm::Module &m ) {
        auto *exit = m.getFunction( "_Exit" );
        auto *suspend = m.getFunction( "__dios_suspend" );
        auto *trace = m.getFunction( "__vm_trace" );

        for ( auto *fn : { exit, suspend } ) {
            if ( !fn || fn->empty() )
                continue;
            llvm::IRBuilder<> irb( &*fn->begin(), fn->begin()->getFirstInsertionPt() );
            irb.CreateCall( trace, { irb.getInt32( _VM_T_LeakCheck ) } );
        }
    }
};

PassMeta leakCheck() {
    return compositePassMeta< LeakCheck >( "leak-check", "Enable memory leak checking" );
}

} // namespace lart::divine
