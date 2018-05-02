// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/stash.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
#include <lart/abstract/util.h>

using namespace lart::abstract;
using namespace llvm;

namespace {

Function* stash_placeholder( Instruction *inst, Type *in ) {
    auto void_type = Type::getVoidTy( inst->getContext() );
	auto fty = llvm::FunctionType::get( void_type, { in }, false );
    std::string name = "lart.stash.placeholder.";
	if ( auto s = dyn_cast< StructType >( in ) )
        name += s->getName().str();
    else
        name += llvm_name( in );
   	return get_or_insert_function( get_module( inst ), fty, name );
}

Function* unstash_placeholder( Instruction *inst, Type *out ) {
	auto fty = llvm::FunctionType::get( out, { inst->getType() }, false );
    std::string name = "lart.unstash.placeholder.";
	if ( auto s = dyn_cast< StructType >( out ) )
        name += s->getName().str();
    else
        name += llvm_name( out );
   	return get_or_insert_function( get_module( inst ), fty, name );
}

} // anonymous namespace

void Stash::run( Module &m ) {
    run_on_abstract_calls( [&] ( auto call ) {
        auto fn = call->getCalledFunction();
        if ( !fn->getMetadata( "lart.abstract.return" ) ) {
            if ( !stashed.count( fn ) ) {
                ret_stash( call );
                arg_unstash( call );
                stashed.insert( fn );
            }
            arg_stash( call );
        }

        ret_unstash( call );
    }, m );
}

void Stash::arg_unstash( CallInst *call ) {
    auto fn = call->getCalledFunction();
    IRBuilder<> irb( fn->getEntryBlock().begin() );

    for ( auto &arg : fn->args() ) {
        auto op = call->getArgOperand( arg.getArgNo() );
        // TODO what if op is not instruction?
        if ( auto inst = dyn_cast< Instruction >( op ) ) {
            if ( has_domain( inst ) && !inst->getType()->isPointerTy() ) {
                auto dom = MDValue( op ).domain();
                auto aty = abstract_type( op->getType(), dom );
                auto unstash_fn = unstash_placeholder( inst, aty );
                irb.CreateCall( unstash_fn, { &arg } );
            }
        }
    }
}

void Stash::arg_stash( CallInst *call ) {
    if ( call->getCalledFunction()->getMetadata( "lart.abstract.return" ) )
        return; // skip internal lart functions

    IRBuilder<> irb( call );
    for ( auto &arg : call->arg_operands() ) {
        // TODO what if op is not instruction?
        if ( auto inst = dyn_cast< Instruction >( arg ) ) {
            if ( has_domain( inst ) && !inst->getType()->isPointerTy() ) {
                auto dom = MDValue( inst ).domain();
                auto aty = abstract_type( inst->getType(), dom );
                auto stash_fn = stash_placeholder( inst, aty );
                if ( isa< CallInst >( inst ) )
                    irb.CreateCall( stash_fn, { get_unstash_placeholder( inst ) } );
                else
                    irb.CreateCall( stash_fn, { get_placeholder( inst ) } );
            }
        }
    }
}

void Stash::ret_stash( CallInst *call ) {
    if ( call->getType()->isVoidTy() )
        return; // no return value to stash

    auto fn = call->getCalledFunction();
    auto ret = dyn_cast< ReturnInst >( fn->back().getTerminator() );
    ASSERT( ret && "Return instruction not found in the last basic block." );

    auto val = ret->getReturnValue();
    auto dom = MDValue( call ).domain();
    auto aty = abstract_type( call->getType(), dom );

    IRBuilder<> irb( ret );
    auto stash_fn = stash_placeholder( ret, aty );

    auto tostash = [&] () -> Value* {
        if ( has_placeholder( val ) )
            return get_placeholder( val );
        else
            return UndefValue::get( aty );
    } ();

    auto stash = irb.CreateCall( stash_fn, { tostash } );
    make_duals( stash, ret );
}

void Stash::ret_unstash( CallInst *call ) {
    if ( call->getType()->isVoidTy() )
        return; // no return value to stash

    auto dom = MDValue( call ).domain();
    auto aty = abstract_type( call->getType(), dom );

    IRBuilder<> irb( call );
    auto unstash_fn = unstash_placeholder( call, aty );
    auto unstash = irb.CreateCall( unstash_fn, { call } );

    call->removeFromParent();
    call->insertBefore( unstash );

    make_duals( unstash, call );
}
