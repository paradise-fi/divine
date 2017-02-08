// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
#pragma once

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
    llvm::AllocaInst * alloca_;
	/* bitcasted value  from alloca */
	llvm::Value * value;

	explicit Annotation ( std::string const &type, llvm::AllocaInst * alloca, llvm::Value * value )
                        : type( type ), alloca_( alloca ), value( value ) { };

};

static llvm::StringRef getAnnotationType( const llvm::CallInst * call ) {
	return llvm::cast< llvm::ConstantDataArray > ( llvm::cast< llvm::GlobalVariable > (
		   call->getOperand( 1 )->stripPointerCasts() )->getInitializer() )->getAsCString();
}

static bool isAbstractAnnotation( const llvm::CallInst * call ) {
	return getAnnotationType( call ).startswith( "lart.abstract" );
}

static Annotation getAnnotation( llvm::CallInst * call ) {
	assert( isAbstractAnnotation( call ) );
	auto type = getAnnotationType( call );
	auto value = call->getOperand( 0 );
	//TODO check uses only in function
    auto alloca = llvm::cast< llvm::AllocaInst >( value->stripPointerCasts() );
	return Annotation( type, alloca, value );
}

static std::vector< Annotation > getAbstractAnnotations( llvm::Module * m ) {
    std::vector< std::string > annotationFunctions = {
        "llvm.var.annotation",
        "llvm.ptr.annotation"
    };
    std::vector< Annotation > annotations;
    for ( const auto & a : annotationFunctions ) {
        for ( auto & f : m->functions() ) {
            if ( f.getName().startswith( a ) )
            for ( auto user : f.users() ) {
                auto call = llvm::cast< llvm::CallInst >( user );
                if ( isAbstractAnnotation( call ) )
                    annotations.push_back( getAnnotation( call ) );
            }
        }
    }
	return annotations;
}

static std::vector< Annotation > getAbstractAnnotations( llvm::Function * fn ) {
    std::vector< Annotation > res;
    auto annots = getAbstractAnnotations( fn->getParent() );
    std::copy_if( annots.begin(), annots.end(), std::back_inserter( res ),
                  [&]( Annotation a ) {
                      return a.alloca_->getParent()->getParent() == fn;
                  } );
    return res;
}

} /* lart */
} /* abstract */
