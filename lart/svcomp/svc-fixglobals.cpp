// -*- C++ -*- (c) 2018 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/util.h>

namespace lart {
namespace svcomp {

struct FixGlobals {

    static PassMeta meta() { return passMeta< FixGlobals >( "fixglobals", "" ); }

    llvm::Function *getNondet( llvm::Type *t, llvm::Module &m )
    {
        if ( auto it = _nondet_map.find( t ); it != _nondet_map.end() )
            return it->second;
        auto sz = t->isPointerTy() ? 64 : t->getScalarSizeInBits();
        sz = sz < 8 ? 8 : sz;
        return _nondet_map.emplace( t, m.getFunction( "__sym_val_i"
                                          + std::to_string( sz ) ) )
                                  .first->second;
    }

    void initializeGlobal( llvm::Value *g, llvm::IRBuilder<> &irb, llvm::Module &m )
    {
        auto *t = llvm::cast< llvm::PointerType >( g->getType() )->getElementType();

        if ( auto *comp = llvm::dyn_cast< llvm::StructType >( t ) ) {
            for ( unsigned i = 0, end = comp->getNumElements(); i < end; ++i )
                initializeGlobal( irb.CreateStructGEP( comp, g, i ), irb, m );
        }
        else if ( auto *arr = llvm::dyn_cast< llvm::ArrayType >( t ) ) {
            for ( unsigned i = 0, end = arr->getNumElements(); i < end; ++i )
                initializeGlobal( irb.CreateConstInBoundsGEP2_64( g, 0, i ), irb, m );
        }
        else {
            auto v = irb.CreateCall( getNondet( t, m ), { } );
            irb.CreateStore( irb.CreateBitOrPointerCast( v, t ), g );
        }
    }

    void run( llvm::Module &m )
    {
        auto &ctx = m.getContext();

        auto *init = llvm::cast< llvm::Function >(
                        m.getOrInsertFunction( "__lart_svc_fixglobals_init",
                            llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), false ) ) );
        auto *initBB = llvm::BasicBlock::Create( ctx, "", init );
        llvm::IRBuilder<> irb( initBB, initBB->getFirstInsertionPt() );

        for ( auto &g : m.globals() ) {
            if ( (g.hasExternalLinkage() || g.hasExternalWeakLinkage())
                 && g.isDeclaration() && !g.isConstant() && !g.getName().startswith( "__md_" ) )
            {
                g.setLinkage( llvm::GlobalValue::InternalLinkage );
                initializeGlobal( &g, irb, m );
                g.setInitializer( llvm::UndefValue::get( g.getType()->getElementType() ) );
            }
        }
        irb.CreateRetVoid();

        auto *mainBB = &*m.getFunction( "main" )->begin();
        irb.SetInsertPoint( mainBB, mainBB->getFirstInsertionPt() );
        irb.CreateCall( init, { } );
    }

  private:
    util::Map< llvm::Type *, llvm::Function * > _nondet_map;
};

PassMeta svcFixGlobalsPass() {
    return compositePassMeta< FixGlobals >( "svc-fixglobals", "" );
}

}
}
