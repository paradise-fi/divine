// -*- C++ -*- (c) 2019 Lukáš Korenčik <xkorenc1@fi.muni.cz>

#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/meta.h>
#include <lart/abstract/util.h>

namespace lart::mcsema
{

template< typename OpCode >
llvm::ConstantExpr *is( llvm::Value *root, OpCode op )
{
    auto constant_expr = llvm::dyn_cast< llvm::ConstantExpr >( root );
    if ( !constant_expr || constant_expr->getOpcode() != op )
        return nullptr;
    return constant_expr;
}

template< typename R, typename C, typename Yield >
void peel( R* root, C* current, Yield yield )
{
    yield( root, current );
}

template< typename R, typename C, typename OpCode, typename ...Args >
void peel( R* root, C* current, OpCode op, Args ...args)
{
    for ( auto user : current->users() )
        if ( auto next = is( user, op ) )
            peel( root, next, args... );
}

/* McSema for reasons sometimes generates constant expressions in form:
 * inttoptr( and ( add ( ptrtoint %global_segment ) ) )
 * These are typically not desired (and divine cannot handle them at the moment),
 * therefore this pass transforms them to equivalent for of
 * inttoptr( add ( ptrtoint %global_segment ) )
 */

struct segment_masks
{
    llvm::Module *module;

    void run( llvm::Module & m  )
    {
        // Remove `and` in between `add` and `inttoptr`
        auto build_bridge = []( auto root, auto end )
        {
            auto bridged = end->getWithOperandReplaced( 0, root );
            end->replaceAllUsesWith( bridged );
        };

        // Find `inttoptr` that is right after `and`
        auto begin_found = [&]( auto root, auto current )
        {
            peel( current, current,
                  llvm::Instruction::And, llvm::Instruction::IntToPtr,
                  build_bridge );
        };

        for ( auto &global : m.globals() )
           peel( &global, &global,
                 llvm::Instruction::PtrToInt,
                 llvm::Instruction::Add,
                 begin_found );
    }
};

}// namespace lart::mcsema
