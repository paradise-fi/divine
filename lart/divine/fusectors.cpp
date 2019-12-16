// -*- C++ -*- (c) 2019 Lukáš Korenčik <xkorenc1@fi.muni.cz>

#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

DIVINE_RELAX_WARNINGS
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
DIVINE_UNRELAX_WARNINGS

#include <bricks/brick-assert>

#include <lart/support/pass.h>
#include <lart/support/meta.h>

namespace lart::divine
{

// One entry in llvm.global_ctors, corresponds to one global ctor/dtor
struct CtorDtorEntry
{
    int32_t prio;
    llvm::Function *func;
    // We don't need last entry here
    // void *ignored;

    CtorDtorEntry( llvm::Constant *entry )
        : prio( entry->getAggregateElement( 0U )->getUniqueInteger().getZExtValue() )
    {

        auto entry_f = entry->getAggregateElement( 1U );
        if ( auto casted_f = llvm::dyn_cast< llvm::Function >( entry_f ) )
            func = casted_f;
        else
        {
            // In case constructor is of diferent type than void (*)() compiler will
            // generate bitcast
            auto const_expr = llvm::dyn_cast< llvm::ConstantExpr >( entry_f );
            if ( const_expr && const_expr->isCast() )
                func = llvm::dyn_cast< llvm::Function >( const_expr->getOperand( 0U ) );
        }

        // func must be set to some value since someone will most likely try
        // to create call to it later
        ASSERT( func );
    }
};

bool compare_ctor( const CtorDtorEntry &lhs, const CtorDtorEntry &rhs )
{
    return lhs.prio < rhs.prio;
}

bool compare_dtor( const CtorDtorEntry &lhs, const CtorDtorEntry &rhs )
{
    return lhs.prio > rhs.prio;
}

// Get all entries from llvm.global_ctors
std::vector< CtorDtorEntry > get_entries( llvm::GlobalVariable *arr )
{
    if (!arr)
        return {};

    std::vector< CtorDtorEntry > result;
    for ( auto i = 0U; auto entry = arr->getInitializer()->getAggregateElement( i ); ++i )
      result.emplace_back(entry);

    return result;
}

void create_explicit_calls(
    llvm::Module &module, const std::vector< CtorDtorEntry >& entries,
    const std::string &name )
{
    auto &ctx = module.getContext();

    auto f_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy( ctx ), false );

    auto func =
        llvm::dyn_cast< llvm::Function >( module.getOrInsertFunction( name, f_type ) );

    auto block = llvm::BasicBlock::Create( ctx, "entry", func );
    llvm::IRBuilder<> irb{ block };

    for ( auto& entry : entries )
    {
        std::vector< llvm::Value * > args;
        for ( auto &a : entry.func->args() )
            args.push_back( llvm::UndefValue::get( a.getType() ) );
        irb.CreateCall( entry.func, args );
    }
    irb.CreateRetVoid();
}

template< typename Comparator >
void fuse_array( llvm::Module &module, const std::string &arr_name,
                 Comparator &&compare, const std::string &func_name )
{
    auto entry_arr = module.getGlobalVariable( arr_name );
    auto entries = get_entries( entry_arr );

    std::sort( entries.begin(), entries.end(), compare );
    create_explicit_calls( module, entries, func_name );
}

// Replaces iteration over llvm.global_ctors and indirect call to each entry with
// functions that explicitly call all entries in proper order
void fuse( llvm::Module &module )
{
    fuse_array( module, "llvm.global_ctors", compare_ctor, "__dios_call_global_ctors");
    fuse_array( module, "llvm.global_dtors", compare_dtor, "__dios_call_global_dtors");
}

struct FuseCtors
{

    void run( llvm::Module &m )
    {
        fuse( m );
    }

};

/* Inserts two new functions into the module:
 * void divine_global_ctors()
 * void divine_global_dtors()
 *
 * These two functions directly call all global constructors (or destructors)
 * in correct order. This is done in order to get rid of indirect calls that are
 * typically present in a function implementing invocation of global ctors/dtors.
 */

PassMeta fuseCtorsPass()
{
    return passMeta< FuseCtors >(
       "fuse-ctors", "Replaces indirect calls to llvm.global_ctors with explicit calls" );
}

} // namespace lart::divine
