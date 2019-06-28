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

    template< typename O >
    void handle_user( llvm::Module &m, llvm::Value *valist, llvm::Value *user, O to_drop )
    {
        auto *load = llvm::dyn_cast< llvm::LoadInst >( user );

        if ( auto *bitcast = llvm::dyn_cast< llvm::BitCastInst >( user ) )
        {
            if ( !bitcast->hasNUses( 1 ) )
                UNREACHABLE( "va_arg bitcast has too many uses:", bitcast );
            load = llvm::dyn_cast< llvm::LoadInst >( *bitcast->user_begin() );
            *to_drop ++ = bitcast;
        }

        if ( !load )
            UNREACHABLE( "could not find the load corresponding to a va_arg use",*user);

        if ( load->getType() == llvm::Type::getX86_FP80Ty( m.getContext() ) )
        {
            auto i8ptr = llvm::Type::getInt8Ty( m.getContext() )->getPointerTo();
            auto i32  = llvm::Type::getInt32Ty( m.getContext() );
            auto voidT = llvm::Type::getVoidTy( m.getContext() );
            auto faultfn_type = llvm::FunctionType::get( voidT, { i32, i8ptr }, false );
            auto faultfn = m.getOrInsertFunction( "__dios_fault", faultfn_type );

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
            irb.CreateCall( faultfn, { id, msg } );
            load->replaceAllUsesWith( llvm::UndefValue::get( load->getType() ) );
            load->eraseFromParent();
        }
        else
            llvm::ReplaceInstWithInst( load, new llvm::VAArgInst( valist, load->getType() ) );
    }

    void run( llvm::Module &m )
    {
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
            if ( call->hasNUses( 0 ) )
            {
                call->eraseFromParent();
                continue;
            }

            auto *valist = call->getArgOperand( 0 );
            brick::data::SmallVector< llvm::Instruction * > to_drop{ call };

            for ( auto user : call->users() )
                handle_user( m, valist, user, std::back_inserter( to_drop ) );

            for ( auto x : to_drop )
            {
                x->replaceAllUsesWith( llvm::UndefValue::get( x->getType() ) );
                x->eraseFromParent();
            }
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
