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

Function* stash_placeholder( Module *m, Type *in ) {
    auto void_type = Type::getVoidTy( m->getContext() );
	auto fty = llvm::FunctionType::get( void_type, { in }, false );
    std::string name = "lart.stash.placeholder.";
	if ( auto s = dyn_cast< StructType >( in ) )
        name += s->getName().str();
    else
        name += llvm_name( in );
   	return get_or_insert_function( m, fty, name );
}

Function* unstash_placeholder( Module *m, Value *val, Type *out ) {
	auto fty = llvm::FunctionType::get( out, { val->getType() }, false );
    std::string name = "lart.unstash.placeholder.";
	if ( auto s = dyn_cast< StructType >( out ) )
        name += s->getName().str();
    else
        name += llvm_name( out );
   	return get_or_insert_function( m, fty, name );
}


} // anonymous namespace

void Stash::run( Module &m ) {
    run_on_abstract_calls( [&] ( auto call ) {
        auto fn = get_called_function( call );
        if ( !fn->getMetadata( "lart.abstract" ) )
            if ( !stashed.count( fn ) )
                arg_unstash( call );
        ret_unstash( call );
        stashed.insert( fn );
    }, m );

    stashed.clear();

    run_on_abstract_calls( [&] ( auto call ) {
        auto fn = get_called_function( call );
        if ( !fn->getMetadata( "lart.abstract" ) )
            if ( !stashed.count( fn ) )
                ret_stash( call );
            arg_stash( call );
        stashed.insert( fn );
    }, m );
}

void Stash::arg_unstash( CallInst *call ) {
    auto fn = get_called_function( call );
    IRBuilder<> irb( &*fn->getEntryBlock().begin() );

    FunctionMetadata fmd{ fn };

    for ( auto &arg : fn->args() ) {
        auto idx = arg.getArgNo();
        auto op = call->getArgOperand( idx );
        auto ty = op->getType();

        if ( !ty->isPointerTy() ) { // TODO is base_type
            auto dom = fmd.get_arg_domain( idx );
            if ( !is_concrete( dom ) ) {
                auto aty = abstract_type( ty, dom );
                auto unstash_fn = unstash_placeholder( get_module( call ), op, aty );
                irb.CreateCall( unstash_fn, { &arg } );
            }
        }
    }
}

void Stash::arg_stash( CallInst *call ) {
    auto fn = get_called_function( call );
    IRBuilder<> irb( call );

    FunctionMetadata fmd{ fn };

    for ( unsigned idx = 0; idx < call->getNumArgOperands(); ++idx ) {
        auto op = call->getArgOperand( idx );
        auto ty = op->getType();

        if ( !ty->isPointerTy() && !isa< Constant >( op ) ) { // TODO is_base_type
            auto dom = fmd.get_arg_domain( idx );

            if ( !is_concrete( dom ) && !is_concrete( op ) ) {
                auto aty = abstract_type( ty, dom );
                auto stash_fn = stash_placeholder( get_module( call ), aty );
                if ( isa< CallInst >( op ) || isa< Argument >( op ) )
                    irb.CreateCall( stash_fn, { get_unstash_placeholder( op ) } );
                else if ( has_placeholder( op ) )
                    irb.CreateCall( stash_fn, { get_placeholder( op ) } );
                else {
                    auto undef = UndefValue::get( stash_fn->getFunctionType()->getParamType( 0 ) );
                    irb.CreateCall( stash_fn, { undef } );
                }
            }
        }
    }
}

void Stash::ret_stash( CallInst *call ) {
    auto fn = get_called_function( call );
    auto retty = fn->getReturnType();
    if ( retty->isVoidTy() || retty->isPointerTy() )
        return; // no return value to stash

    auto rets = query::query( *fn ).flatten()
        .map( query::refToPtr )
        .filter( query::llvmdyncast< ReturnInst > )
        .freeze();

    ASSERT( rets.size() == 1 && "No single return instruction found." );
    auto ret = cast< ReturnInst >( rets[ 0 ] );

    auto val = ret->getReturnValue();
    auto dom = MDValue( call ).domain();
    auto aty = abstract_type( call->getType(), dom );

    IRBuilder<> irb( ret );
    auto stash_fn = stash_placeholder( get_module( call ), aty );

    Value *tostash = nullptr;
    if ( has_placeholder( val ) )
        tostash = get_placeholder( val );
    else if ( isa< Argument >( val ) || isa< CallInst >( val ) )
            tostash = get_unstash_placeholder( val );
    else
        tostash = UndefValue::get( aty );

    auto stash = irb.CreateCall( stash_fn, { tostash } );
    make_duals( stash, ret );
}

void Stash::ret_unstash( CallInst *call ) {
    auto fn = get_called_function( call );
    auto retty = fn->getReturnType();
    if ( retty->isVoidTy() || retty->isPointerTy() )
        return; // no return value to stash

    auto dom = MDValue( call ).domain();
    auto aty = abstract_type( call->getType(), dom );

    IRBuilder<> irb( call );
    auto unstash_fn = unstash_placeholder( get_module( call ), call, aty );
    auto unstash = irb.CreateCall( unstash_fn, { call } );

    call->removeFromParent();
    call->insertBefore( unstash );

    make_duals( unstash, call );
}
