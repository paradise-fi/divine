// -*- C++ -*- (c) 2019 Lukáš Korenčik <xkorenc1@fi.muni.cz>

#pragma once

#include <iostream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <bricks/brick-assert>
#include <bricks/brick-llvm>

#include <lart/support/meta.h>
#include <lart/abstract/util.h>

namespace lart::mcsema
{

/* McSema generates entrypoints into lifted code in form of function
 * with fixed number of i64 arguments. Therefore we need to transform
 * callsites that do call these entrypoints indirectly to bitcast
 * callee to matching arity.
 */
struct libc_indirect_calls : abstract::LLVMUtil< libc_indirect_calls >
{
    llvm::Module *module;

    static constexpr const int explicit_args_count = 8;

    void run( llvm::Module &m )
    {
        module = &m;
        auto f = get_caller();

        // We did not find the function, it is possible it is not linked in
        // therefore there is not much left for us to do
        if ( !f )
            return;
        fill_arguments( *f, explicit_args_count );
    }

    // Finds indirect call in the functions and adds i64 undef arguments to it
    // up to args_size
    void fill_arguments( llvm::Function &f, uint64_t args_size )
    {
        auto indirect_calls =
            query::query( f )
            .flatten()
            .filter( []( auto &inst )
               {
                   auto cs = llvm::CallSite{ &inst };
                   return cs && cs.isIndirectCall();
               } )
            .map( []( auto &inst ){ return &inst; } )
            .freeze();

        for ( auto c_inst : indirect_calls )
        {
            auto cs = llvm::CallSite{ c_inst };

            // It is indirect call -> called value is not function -> cannot use
            // getFunctionType() directly
            auto callee = cs.getCalledValue();
            auto callee_p = llvm::dyn_cast< llvm::PointerType >( callee->getType() );
            auto callee_t = llvm::dyn_cast< llvm::FunctionType >(
                    callee_p->getElementType() );
            ASSERT( callee_t );

            std::vector< llvm::Type * > args{ callee_t->params() };
            for ( auto i = args.size(); i < args_size; ++i )
                args.push_back( i64Ty() );

            auto correct_type = llvm::FunctionType::get( callee_t->getReturnType(),
                                                         args, false );
            llvm::IRBuilder<> ir( c_inst );
            auto casted_callee = ir.CreateBitCast( callee, ptr( correct_type ) );

            std::vector< llvm::Value * > new_args{ cs.arg_begin(),
                                                   cs.arg_end() };
            for ( auto i = cs.arg_size(); i < args_size; ++i )
                new_args.push_back( undef( i64Ty() ) );

            auto new_call = ir.CreateCall( casted_callee, new_args );
            c_inst->replaceAllUsesWith( new_call );
            c_inst->eraseFromParent();
        }

    }

    llvm::Function *get_caller()
    {
        return module->getFunction( "__pthread_start" );
    }

};


}// namespace lart::mcsema
