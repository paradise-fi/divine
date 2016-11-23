// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_RELAX_WARNINGS

#include <string>
#include <cassert>

namespace lart {
namespace abstract {

struct Annotation {
    /* name of desired abstraction */
    std::string type;
	/* corresponding alloca to annotated variable */
    llvm::AllocaInst * allocaInst;
	/* bitcasted value  from alloca*/
	llvm::Value * value;

	explicit Annotation ( std::string const &type, llvm::AllocaInst * alloca, llvm::Value * value )
                        : type( type ), allocaInst( alloca ), value( value ) { };

};

llvm::StringRef getAnnotationType( const llvm::CallInst * call ) {
	assert( call->getCalledFunction()->getName().str() == "llvm.var.annotation" );
	return llvm::cast< llvm::ConstantDataArray > ( llvm::cast< llvm::GlobalVariable > (
		   call->getOperand( 1 )->stripPointerCasts() )->getInitializer() )->getAsCString();
}

bool isAbstractAnnotation( const llvm::CallInst * call ) {
	assert( call->getCalledFunction()->getName().str() == "llvm.var.annotation" );
	return getAnnotationType( call ).startswith( "lart.abstract" );
}

Annotation getAnnotation( llvm::CallInst * call ) {
	assert( isAbstractAnnotation( call ) );
	auto type = getAnnotationType( call );
	auto value = call->getOperand( 0 );
	//TODO check uses only in function
    auto alloca = llvm::cast< llvm::AllocaInst >( value->stripPointerCasts() );
	return Annotation( type, alloca, value );
}

std::vector< Annotation > getAbstractAnnotations( llvm::Module & m ) {
	auto f =  m.getFunction( "llvm.var.annotation" );
	std::vector< Annotation > annotations;

	if ( f != nullptr )
    for ( auto user : f->users() ) {
		auto call = llvm::cast< llvm::CallInst >( user );
		if ( isAbstractAnnotation( call ) )
			annotations.push_back( getAnnotation( call ) );
	}
	return annotations;
}

} /* lart */
} /* abstract */
