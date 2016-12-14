// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/abstraction.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/walker.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

#include <lart/abstract/types.h>

namespace lart {
namespace abstract {

namespace {
    bool isLifted( const std::vector< llvm::Value * > & deps ) {
        for ( auto & dep : deps )
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( dep ) )
                if ( intrinsic::isLift( call ) ) return true;
        return false;
    }
}

void Abstraction::init( llvm::Module & m ) {
    auto & ctx = m.getContext();
    auto tristate = Tristate::get( ctx );
    builder.store( llvm::IntegerType::getInt1Ty( ctx ), tristate );

    walker = AbstractWalker( m );
}

llvm::PreservedAnalyses Abstraction::run( llvm::Module & m ) {
    init( m );

    for ( auto fn : walker.functions() ) {
        preprocess( fn );
		process( fn, walker.entries( fn ) );
    }

    // clean functions
	for ( auto & f : _unused )
		f->eraseFromParent();

    return llvm::PreservedAnalyses::none();
}

void Abstraction::preprocess( llvm::Function * fn ) {
    // change switches to branching
    auto lspass = llvm::createLowerSwitchPass();
    lspass->runOnFunction( *fn );
    llvm::UnifyFunctionExitNodes ufen;
    ufen.runOnFunction( *fn );
}

void Abstraction::process( llvm::Argument * a ) {
    builder.process( a );
}

void Abstraction::process( llvm::Instruction * i ) {
    builder.process( i );
}

void Abstraction::processAllocas( llvm::Function * fn ) {
    auto allocas = query::query( *fn ).flatten()
                  .map( query::refToPtr )
                  .map( query::llvmdyncast< llvm::AllocaInst > )
                  .filter( query::notnull )
                  .freeze();

    for ( auto & a : allocas )
        if ( isLifted( walker.dependencies( a ) ) )
            propagate( a );
}

void Abstraction::process( llvm::Function * fn,
						   std::vector< llvm::Value * > const& entries) {
	for ( const auto &  entry : entries )
        propagate( entry );

    processAllocas( fn );

    auto changed = changeReturn( fn );
	builder.store( fn, changed );
	builder.annotate();

	if ( fn != changed ) {
        std::vector< llvm::User * > users = { fn->user_begin(), fn->user_end() } ;
        for ( auto user : users ) {
			if ( auto inst = llvm::dyn_cast< llvm::Instruction >( user ) ) {
				auto parent = inst->getParent()->getParent();
				preprocess( parent );
				process( parent, { inst } );
			}
		}
		_unused.insert( fn );
	}
}

void Abstraction::propagateFromCall( llvm::CallInst * call ) {
    auto types = builder.arg_types( call );
    auto fn = call->getCalledFunction();
    auto fty = llvm::FunctionType::get( fn->getFunctionType()->getReturnType(),
                                        types,
                                        fn->getFunctionType()->isVarArg() );
    llvm::Function* clone = lart::cloneFunction( fn, fty );

    preprocess( clone );

    auto entries = query::query( clone->args() )
				  .map( query::refToPtr )
                  .filter( [&]( llvm::Argument * arg ) {
                      return types::isAbstract( arg->getType() );
                  } ).freeze();

    for ( const auto & entry : entries )
        propagate( entry );

    processAllocas( clone );

    auto changed = changeReturn( clone );
	builder.store( fn, changed );
    if ( changed != clone )
        clone->eraseFromParent();
}

void Abstraction::propagate( llvm::Value * entry ) {
    if ( !types::isAbstract( entry->getType() ) )
        builder.store( entry->getType(), types::lift( entry->getType() ) );
    auto deps = walker.dependencies( entry );

    for ( const auto & dep : lart::util::reverse( deps ) ) {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( dep ) )
            if ( builder.needToPropagate( call ) )
                propagateFromCall( call );
        if ( auto i = llvm::dyn_cast< llvm::Instruction >( dep ) )
            process( i );
        else if ( auto a = llvm::dyn_cast< llvm::Argument >( dep ) )
            process( a );
    }

    // builder cleans unused dependencies and lifts
    builder.clean( deps );
}

llvm::Function * Abstraction::changeReturn( llvm::Function * fn ) {
    bool changeReturn = false;
    if ( !types::isAbstract( fn->getReturnType() ) ) {
        changeReturn = query::query( *fn ).flatten()
					  .map( query::refToPtr )
					  .map( query::llvmdyncast< llvm::ReturnInst > )
					  .filter( query::notnull )
					  .any( [&]( llvm::ReturnInst * ret ) {
					        return types::isAbstract( ret->getReturnValue()
														 ->getType() );
					  } );
    }

	return changeReturn ? builder.changeReturn( fn ) : fn;
}


} // abstract
} // lart

