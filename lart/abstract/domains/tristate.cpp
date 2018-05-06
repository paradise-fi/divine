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
    return m->getFunction( "__tristate_lower" )->arg_begin()->getType();
}

Value* impl_tobool( CallInst *call, Values &args ) {
    IRBuilder<> irb( call->getContext() );
    auto fn = get_module( call )->getFunction( "__tristate_lower" );
    return irb.CreateCall( fn, args );
}

Value * Tristate::process( Instruction *intr, Values &args ) {
    auto call = cast< CallInst >( intr );
    if ( is_tobool( call ) )
        return impl_tobool( call, args );
    UNREACHABLE( "Unsupported operation on tristate." );
}

Value* Tristate::lift( Value * ) {
    NOT_IMPLEMENTED();
}

Type* Tristate::type( Module *, Type * ) const {
    NOT_IMPLEMENTED();
}

Value *Tristate::default_value( Type * ) const {
    NOT_IMPLEMENTED();
}

} // namespace abstract
} // namespace lart
