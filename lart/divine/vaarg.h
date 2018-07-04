// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>
#include <brick-string>
#include <brick-data>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/util.h>

namespace lart {
namespace divine {

struct VaArgInstr
{
    static PassMeta meta() {
        return passMeta< VaArgInstr >( "VaArgInstr", "" );
    }

    void run( llvm::Module &m ) {
        auto *vaargfn = m.getFunction( "__lart_llvm_va_arg" );
        if ( !vaargfn )
            return;
        if ( vaargfn->hasAddressTaken() )
            UNREACHABLE( "__lart_llvm_va_arg has address taken" );

        for ( auto *v : vaargfn->users() )
            if ( !llvm::isa< llvm::CallInst >( v ) )
                UNREACHABLE( "all uses of __lart_llvm_va_arg have to be calls:", v );

        for ( auto *call : query::query( vaargfn->users() )
                            .map( query::llvmcast< llvm::CallInst > )
                            .freeze() /* avoid messing with iterators when we modify BBs */ )
        {
            if ( call->hasNUses( 0 ) ) {
                call->eraseFromParent();
                continue;
            }

            if ( call->hasNUsesOrMore( 2 ) )
                UNREACHABLE( "call to __lart_llvm_va_arg must have at most one use:", call );

            brick::data::SmallVector< llvm::Instruction * > toDrop{ call };
            auto *valist = call->getArgOperand( 0 );
            auto *user = *call->user_begin();
            auto *load = llvm::dyn_cast< llvm::LoadInst >( user );
            if ( auto *bitcast = llvm::dyn_cast< llvm::BitCastInst >( user ) ) {
                if ( !bitcast->hasNUses( 1 ) )
                    UNREACHABLE( "va_arg bitcast has too many uses:", bitcast );
                load = llvm::dyn_cast< llvm::LoadInst >( *bitcast->user_begin() );
                toDrop.push_back( bitcast );
            }
            if ( !load )
                UNREACHABLE( "could not find load corresponding to va_arg call", call );

            for ( auto x : toDrop ) {
                x->replaceAllUsesWith( llvm::UndefValue::get( x->getType() ) );
                x->eraseFromParent();
            }
            llvm::ReplaceInstWithInst( load, new llvm::VAArgInst( valist, load->getType() ) );
        }

        vaargfn->eraseFromParent();
    }
};

PassMeta vaArgPass() {
    return compositePassMeta< VaArgInstr >( "vaarg",
            "Convert intrinsics into use of va_arg instruction" );
}

} // namespace divine
} // namespace lart
