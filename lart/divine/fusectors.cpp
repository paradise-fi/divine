// -*- C++ -*- (c) 2019 Lukáš Korenčik <xkorenc1@fi.muni.cz>

#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

DIVINE_RELAX_WARNINGS
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
DIVINE_UNRELAX_WARNINGS

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

  CtorDtorEntry( llvm::Constant *entry ) :
      prio( entry->getAggregateElement( 0U )->getUniqueInteger().getZExtValue() ),
      func( llvm::dyn_cast< llvm::Function >( entry->getAggregateElement( 1U ) ) ) {}
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
std::vector< CtorDtorEntry > get_ctors( llvm::GlobalVariable *arr )
{
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

    auto entry = llvm::BasicBlock::Create( ctx, "entry", func );
    llvm::IRBuilder<> irb{ entry };

    for ( auto& entry : entries )
        irb.CreateCall( entry.func );

    irb.CreateRetVoid();
}

// Replaces iteration over llvm.global_ctors and indirect call to each entry with
// functions that explicitly call all entries in proper order
void fuse_ctors( llvm::Module &module )
{
    auto ctor_arr = module.getGlobalVariable( "llvm.global_ctors" );

    auto entries = get_ctors( ctor_arr );

    // This can be done smarter but since array is small it does not really matter
    std::sort( entries.begin(), entries.end(), compare_ctor ),
    create_explicit_calls(
        module,
        entries,
        "divine_global_ctors_init" );

    std::sort( entries.begin(), entries.end(), compare_dtor ),
    create_explicit_calls(
        module,
        entries,
        "divine_global_dtors_fini" );
}

struct FuseCtors
{

    void run( llvm::Module &m )
    {
      fuse_ctors( m );
    }

};

PassMeta fuseCtorsPass()
{
    return passMeta< FuseCtors >(
       "fuse-ctors", "Replaces indirect calls to llvm.global_ctors with explicit calls" );
}

} // namespace lart::divine
