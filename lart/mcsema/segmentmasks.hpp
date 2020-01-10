// -*- C++ -*- (c) 2019 Lukáš Korenčik <xkorenc1@fi.muni.cz>

#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/meta.h>

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
 *
 * inttoptr( and ( add ( ptrtoint %[global_segment, function] ) ) )
 * and ( add ( ptrtoint %global_segment ) )
 *
 * These are typically not desired (and divine cannot handle them at the moment),
 * therefore this pass transforms them to equivalent form of
 * inttoptr( add ( ptrtoint %global_segment ) )
 */

struct segment_masks
{
    llvm::Module *module;

    // Change first operand of `end` to `root`. This effectively removes anything
    // between them.
    static auto build_bridge()
    {
        return []( auto root, auto end )
        {
            auto bridged = end->getWithOperandReplaced( 0, root );
            end->replaceAllUsesWith( bridged );
        };
    }

    // Remove layers between `root` and `end`
    static auto remove()
    {
        return []( auto root, auto end )
        {
            end->replaceAllUsesWith( root );
        };
    }

    // Move in expression by `args`
    template< typename Yield, typename ...Args >
    static auto peel_some( Yield yield, Args ...args )
    {
        return [=]( auto root, auto current )
        {
            peel( current, current, args..., yield );
        };
    }

    void functions( llvm::Module &m )
    {
        auto find_bridge_begin = peel_some( build_bridge(),
                                            llvm::Instruction::And,
                                            llvm::Instruction::IntToPtr );

        for ( auto &func : m )
            peel( &func, &func,
                  llvm::Instruction::PtrToInt,
                  find_bridge_begin );

    }

    void globals( llvm::Module &m )
    {
        auto find_bridge_begin = peel_some( build_bridge(),
                                            llvm::Instruction::And,
                                            llvm::Instruction::IntToPtr );

        auto peel_outside = peel_some( remove(),
                                       llvm::Instruction::Add,
                                       llvm::Instruction::And );

        for ( auto &global : m.globals() )
        {
            peel( &global, &global,
                  llvm::Instruction::PtrToInt,
                  llvm::Instruction::Add,
                  find_bridge_begin );

            peel( &global, &global,
                  llvm::Instruction::PtrToInt,
                  peel_outside );
        }
    }


    void run( llvm::Module &m  )
    {
        functions( m );
        globals( m );
    }
};

} // namespace lart::mcsema
