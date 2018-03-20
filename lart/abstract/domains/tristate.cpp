// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/tristate.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

using namespace llvm;

namespace lart {
namespace abstract {

Type* tristate_t( Module *m ) {
    return m->getFunction( "__abstract_tristate_lower" )->arg_begin()->getType();
}

Value* impl_rep( CallInst *call ) {
    IRBuilder<> irb( call );
    auto val = call->getOperand( 0 );
    auto aty = tristate_t( get_module( call ) );
    auto ev = irb.CreateExtractValue( val, 0 );
    return irb.CreateZExt( ev, aty );
}

Value* impl_unrep( CallInst *call, Values &args ) {
    IRBuilder<> irb( call );
    auto ty = cast< StructType >( call->getType() )->getElementType( 0 );
    auto tr = irb.CreateTrunc( args[ 0 ], ty );
	auto cs = Constant::getNullValue( call->getType() );
    auto unrep = irb.CreateInsertValue( cs, tr, 0 );
    call->replaceAllUsesWith( unrep );
    return unrep;
}

Value* impl_lift( CallInst *call ) {
    IRBuilder<> irb( call );
    auto op = call->getOperand( 0 );
    auto fn = get_module( call )->getFunction( "__abstract_tristate_lift" );
    auto tr = irb.CreateTrunc( op, fn->arg_begin()->getType() );
    return irb.CreateCall( fn, { tr } );
}

Value* impl_lower( CallInst *call, Values &args ) {
    IRBuilder<> irb( call );
    auto fn = get_module( call )->getFunction( "__abstract_tristate_lower" );
    auto lower = irb.CreateCall( fn, args );
    auto zext = irb.CreateZExt( lower, get_function( call )->getReturnType() );
    call->replaceAllUsesWith( zext );
    return zext;
}

Value * Tristate::process( Instruction *intr, Values &args ) {
    auto call = cast< CallInst >( intr );
    if ( is_lift( call ) )
        return impl_lift( call );
    if ( is_lower( call ) )
        return impl_lower( call, args );
    if ( is_rep( call ) )
        return impl_rep( call );
    if ( is_unrep( call ) )
        return impl_unrep( call, args );
    UNREACHABLE( "Unsupported operation on tristate." );
}

} // namespace abstract
} // namespace lart
