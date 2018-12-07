// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/branching.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Argument.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

using namespace llvm;

using lart::util::get_module;
using lart::util::get_or_insert_function;

void ExpandBranching::run( Module &m ) {
    auto branched_on = [] ( Value* val ) {
        return query::query( val->users() ).any( [] ( auto u ) { return isa< BranchInst >( u ); } );
    };

    for ( auto abstract : placeholders( m ) ) {
        if ( branched_on( abstract->getOperand( 0 ) ) )
            expand_to_i1( abstract );
    }
}

Function* to_i1_placeholder( Instruction* abstract ) {
    auto i1 = Type::getInt1Ty( abstract->getContext() );
	auto fty = llvm::FunctionType::get( i1, { abstract->getType() }, false );
    std::string name = "lart.to_i1.placeholder." + cast< StructType >( abstract->getType() )->getName().str();
   	return get_or_insert_function( get_module( abstract ), fty, name );
}

void ExpandBranching::expand_to_i1( Instruction *abstract ) {
    IRBuilder<> irb( abstract );
    auto to_i1_fn = to_i1_placeholder( abstract );
    auto to_i1 = irb.CreateCall( to_i1_fn, { abstract } );
    add_abstract_metadata( to_i1, get_domain( abstract ) );

    abstract->removeFromParent();
    abstract->insertBefore( to_i1 );

    for ( auto u : abstract->getOperand( 0 )->users() )
        if ( auto br = dyn_cast< BranchInst >( u ) )
            br->setCondition( to_i1 );
}

} // namespace abstract
} // namespace lart

