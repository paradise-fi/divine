// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/duplicator.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/util.h>


using namespace llvm;

using lart::util::get_module;
using lart::util::get_or_insert_function;

namespace lart::abstract
{

Function* placeholder( Module *m, Type *in, Type *out ) {
	auto fty = llvm::FunctionType::get( out, { in }, false );
    std::string name = "lart.placeholder.";
    if ( auto s = dyn_cast< StructType >( out ) )
        name += s->getName().str();
    else
        name += llvm_name( out );
   	return get_or_insert_function( m, fty, name );
}

void Duplicator::run( llvm::Module &m ) {
	auto abstract = query::query( abstract_metadata( m ) )
	    .map( [] ( auto mdv ) { return mdv.value(); } )
	    .map( query::llvmdyncast< Instruction > )
	    .filter( is_transformable )
	    .freeze();

    for ( const auto &inst : abstract )
    	process( inst );
}

void Duplicator::process( llvm::Instruction *i ) {
    auto m = get_module( i );

    auto dom = ValueMetadata( i ).domain();
    auto type = is_base_type_in_domain( m, i, dom )
              ? abstract_type( i->getType(), dom ) : i->getType();

    auto place = isa< PHINode >( i ) ? i->getParent()->getFirstNonPHI() : i;

    IRBuilder<> irb( place );
    auto fn = placeholder( m, i->getType(), type );
    auto ph = irb.CreateCall( fn, { i } );
    add_abstract_metadata( ph, dom );

    if ( !isa< PHINode >( i ) ) {
        ph->removeFromParent();
        ph->insertAfter( place );
    }

    make_duals( i, ph );

    if ( !is_base_type_in_domain( m, i, dom ) ) {
        i->replaceAllUsesWith( ph );
        ph->setOperand( 0, i );
    }
}

}
