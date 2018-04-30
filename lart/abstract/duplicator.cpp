// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/duplicator.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/abstract/metadata.h>

using namespace llvm;
using namespace lart::abstract;

namespace {

bool is_duplicable( Value *v ) {
    return is_one_of< BinaryOperator, CmpInst, TruncInst,
                      SExtInst, ZExtInst, LoadInst, PHINode >( v );
}

Function* dup_function( Module *m, Type *in, Type *out ) {
	auto fty = llvm::FunctionType::get( out, { in }, false );
    auto name = "lart.placeholder." + llvm_name( out );
   	return get_or_insert_function( m, fty, name );
}

} // anonymous namespace

void Duplicator::run( llvm::Module &m ) {
	auto abstract = query::query( abstract_metadata( m ) )
	    .filter( [] ( auto mdv ) {
            return is_base_type( mdv.value()->getType() );
        } )
	    .filter( [] ( auto mdv ) {
            if ( auto icmp = dyn_cast< ICmpInst >( mdv.value() ) ) {
                return query::query( icmp->operands() ).all( [] ( auto &op ) {
                    return is_base_type( op->getType() );
                } );
            } else {
                return true;
            }
        } )
        .freeze();

    for ( const auto & mdv : abstract )
        if ( auto call = dyn_cast< CallInst >( mdv.value() ) )
            process_call( call );

    for ( const auto & mdv : abstract )
		if ( is_duplicable( mdv.value() ) )
    		process( cast< Instruction >( mdv.value() ) );
}

void Duplicator::process_call( CallInst *call ) {
    auto fn = call->getCalledFunction();
    if ( seen.count( fn ) )
        return;
    if ( fn->isIntrinsic() )
        return;

    IRBuilder<> irb( fn->getEntryBlock().begin() );

    auto m = get_module( call );
    auto &ctx = call->getContext();
    auto dom = MDValue( call ).domain();

    if ( fn->getMetadata( "lart.abstract.roots" ) ) {
        for ( auto &arg : fn->args() ) {
            auto aty = abstract_type( arg.getType(), dom ); // TODO get domain from args of call
            auto dup = dup_function( m, arg.getType(), aty );
            auto a = irb.CreateCall( dup, { &arg } );

            add_domain_metadata( a, dom );
            a->setMetadata( "lart.dual", MDTuple::get( ctx, { ValueAsMetadata::get( &arg ) } ) );
        }
    }
}

void Duplicator::process( llvm::Instruction *i ) {
    ASSERT( i->getType()->isIntegerTy() );
    auto dom = MDValue( i ).domain();
    auto type = abstract_type( i->getType(), dom );
    IRBuilder<> irb( i );
    auto dup = [&] () -> Instruction* {
        if ( auto phi = dyn_cast< PHINode >( i ) ) {
            return irb.CreatePHI( type, phi->getNumIncomingValues() );
        } else {
            auto fn = dup_function( get_module( i ), i->getType(), type );
            return irb.CreateCall( fn, { i } );
        }
    } ();

    dup->removeFromParent();
    dup->insertAfter( i );

    make_duals( i, dup );
}
