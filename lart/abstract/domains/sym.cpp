// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/sym.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

#include <iostream>
namespace lart {
namespace abstract {

using namespace llvm;

const std::string Symbolic::name_pref = "__sym_";

namespace {

Type* formula_t( Module *m ) {
    return m->getFunction( Symbolic::name_pref + "lift" )->getReturnType();
}

ConstantInt* bitwidth( Value *v ) {
    auto &ctx = v->getContext();
    auto ty = v->getType();
    auto bw = cast< IntegerType >( ty )->getBitWidth();
    return ConstantInt::get( IntegerType::get( ctx, 32 ), bw );
}

ConstantInt* cast_bitwidth( Function *fn ) {
    auto &ctx = fn->getContext();
    auto bw = fn->getName().rsplit('.').second.drop_front().str();
    return ConstantInt::get( IntegerType::get( ctx, 32 ), std::stoi( bw ) );
}

std::string intr_name( CallInst *call ) {
    auto intr = call->getCalledFunction()->getName();
    size_t pref = std::string( "__vm_test_taint.lart.sym." ).length();
    auto tag = intr.substr( pref ).split( '.' ).first;
    return Symbolic::name_pref + tag.str();
}

Value* impl_rep( CallInst *call, Values &args  ) {
    IRBuilder<> irb( call->getContext() );
    auto name = "__sym_peek_formula";
    auto peek = get_module( call )->getFunction( name );
    return irb.CreateCall( peek, args );
}

Value* impl_unrep( CallInst *call, Values &args  ) {
    IRBuilder<> irb( call->getContext() );
    auto name = "__sym_poke_formula";
    auto poke = get_module( call )->getFunction( name );
    return irb.CreateCall( poke, args );
}

Value* impl_assume( CallInst *call, Values &args ) {
    IRBuilder<> irb( call->getContext() );
    auto fn = get_module( call )->getFunction( Symbolic::name_pref + "assume" );
    return irb.CreateCall( fn, { args[ 1 ], args[ 1 ], args[ 2 ] } );
}

Value* impl_tobool( CallInst *call, Values &args ) {
    IRBuilder<> irb( call->getContext() );
    auto name = Symbolic::name_pref + "bool_to_tristate";
    auto fn = get_module( call )->getFunction( name );
    return irb.CreateCall( fn, args );
}


Value* impl_cast( CallInst *call, Values &args ) {
    IRBuilder<> irb( call->getContext() );
    auto name = intr_name( call );
    auto op = cast< Function >( call->getOperand( 0 ) );

    auto fn = get_module( call )->getFunction( name );
    return irb.CreateCall( fn, { args[ 0 ], cast_bitwidth( op ) } );
}

Value* impl_op( CallInst *call, Values &args ) {
    IRBuilder<> irb( call->getContext() );
    auto name = intr_name( call );
    auto fn = get_module( call )->getFunction( name );
    return irb.CreateCall( fn, args );
}

} // anonymous namespace

Value* Symbolic::process( Instruction *intr, Values &args ) {
    auto call = cast< CallInst >( intr );

    if ( is_rep( call ) )
        return impl_rep( call, args );
    if ( is_unrep( call ) )
        return impl_unrep( call, args );
    if ( is_cast( call ) )
        return impl_cast( call, args );
    if ( is_assume( call ) )
        return impl_assume( call, args );
    if ( is_tobool( call ) )
        return impl_tobool( call, args );
    return impl_op( call, args );
}

Value* Symbolic::lift( Value *v ) {
    auto &ctx = v->getContext();
    IRBuilder<> irb( ctx );
    auto fn = get_module( v )->getFunction( Symbolic::name_pref + "lift" );
    auto val = irb.CreateSExt( v, IntegerType::get( ctx, 64 ) );
    auto argc = ConstantInt::get( IntegerType::get( ctx, 32 ), 1 );
    return irb.CreateCall( fn, { bitwidth( v ), argc, val } );
}

Type* Symbolic::type( Module *m, Type * ) const {
    auto fn = m->getFunction( Symbolic::name_pref + "lift" );
    return fn->getReturnType();
}

} // namespace abstract
} // namespace lart
