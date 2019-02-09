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

#include <divine/vm/divm.h>

namespace lart {
namespace divine {

struct VaArgInstr
{
    static PassMeta meta() {
        return passMeta< VaArgInstr >( "VaArgInstr", "" );
    }

    void run( llvm::Module &m ) {
        auto *vaargfn = m.getFunction( "__lart_llvm_va_arg" );
        auto i8ptr = llvm::Type::getInt8Ty( m.getContext() )->getPointerTo();
        auto i32  = llvm::Type::getInt32Ty( m.getContext() );
        auto voidT = llvm::Type::getVoidTy( m.getContext() );
        auto faultfn_type = llvm::FunctionType::get( voidT, { i32, i8ptr }, false );
        auto faultfn = m.getOrInsertFunction( "__dios_fault", faultfn_type );
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

            for ( auto x : toDrop )
            {
                x->replaceAllUsesWith( llvm::UndefValue::get( x->getType() ) );
                x->eraseFromParent();
            }

            if ( load->getType() == llvm::Type::getX86_FP80Ty( m.getContext() ) )
            {
                llvm::IRBuilder irb( load );
                auto mkstr = [&]( auto s )
                {
                    return llvm::ConstantDataArray::getString( m.getContext(), s );
                };
                auto str = mkstr( "va_arg for long double is not implemented" );
                auto var_ = m.getOrInsertGlobal( "lart.vaarg.fp80.na", str->getType() );
                auto var = llvm::cast< llvm::GlobalVariable >( var_ );
                auto msg = llvm::ConstantExpr::getPointerCast( var, i8ptr );
                auto id = llvm::ConstantInt::get( i32, _VM_F_NotImplemented );

                var->setInitializer( str );
                var->setConstant( true );
                var->dump();
                irb.CreateCall( faultfn, { id, msg } );
                load->replaceAllUsesWith( llvm::UndefValue::get( load->getType() ) );
                load->eraseFromParent();
            }
            else
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
