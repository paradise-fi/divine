// -*- C++ -*- (c) 2015-2016 Vladimír Štill <xstill@fi.muni.cz>
#include <lart/divine/stubs.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS
#include <brick-string>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

#include <divine/vm/divm.h>

namespace lart {
namespace divine {

PassMeta DropEmptyDecls::meta() {
    return passMeta< DropEmptyDecls >( "DropEmptyDecls", "Remove unused function declarations." );
}

void DropEmptyDecls::run( llvm::Module &m ) {
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
    if ( toDrop.size() )
        std::cerr << "INFO: erased " << toDrop.size() << " empty declarations out of " << all << std::endl;
}



struct StubDecls {

    static PassMeta meta() {
        return passMeta< StubDecls >( "StubDecls", "Replace non-intrinsic function declarations with definitions call to __dios_fault( _VM_F_NotImplemented )" );
    }

    void run( llvm::Module &m ) {
        auto &ctx = m.getContext();
        auto problem = m.getFunction( "__dios_fault" );
        ASSERT( problem );
        auto ptype = problem->getFunctionType()->getParamType( 0 );
        auto pstrty = problem->getFunctionType()->getParamType( 1 );

        const auto undefstr = "lart.divine.stubs: Function stub called.";
        auto undefInit = llvm::ConstantDataArray::getString( ctx, undefstr );
        auto undef = llvm::cast< llvm::GlobalVariable >(
                m.getOrInsertGlobal( "lart.divine.stubs.undefined.str", undefInit->getType() ) );
        undef->setConstant( true );
        undef->setInitializer( undefInit );

        long all = 0;
        for ( auto &fn : m ) {
            if ( fn.isDeclaration() && !fn.isIntrinsic() && !fn.getName().startswith( "__vm_" ) ) {
                ++all;
                auto bb = llvm::BasicBlock::Create( ctx, "", &fn );
                llvm::IRBuilder<> irb( bb );
                auto undefPt = irb.CreateBitCast( undef, pstrty );
                irb.CreateCall( problem, { llvm::ConstantInt::get( ptype, int( _VM_F_NotImplemented ) ), undefPt } );
                irb.CreateUnreachable();
            }
        }
        if ( all )
            std::cerr << "INFO: stubbed " << all << " declarations" << std::endl;
    }
};

PassMeta stubsPass() {
    return compositePassMeta< DropEmptyDecls, StubDecls >( "stubs", "Remove unused function declarations and add __dios_fault to the remaining non-intrinsic declarations" );
}

}
}
