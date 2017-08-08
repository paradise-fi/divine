// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/types/utils.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/composed.h>
#include <lart/abstract/types/scalar.h>

namespace lart {
namespace abstract {

AbstractTypePtr getFromLLVM( Type * type, DomainPtr dom = nullptr ) {
    auto tn = TypeName( llvm::cast< llvm::StructType >( stripPtr( type ) ) );
    auto origin = tn.base().llvm( type->getContext() );
    if ( type->isPointerTy() )
        origin = origin->getPointerTo();
    if ( dom )
        return liftType( origin, dom );
    else
        return liftType( origin, tn.domain() );
}

AbstractTypePtr liftType( Type * type, DomainPtr dom ) {
    // Force lift to be idempotent
    if ( isAbstract( stripPtr( type ) ) )
        return getFromLLVM( type );
    return dom->match(
        [&] ( const UnitDomain & ) -> AbstractTypePtr {
            return ScalarType::make( type, dom );
        },
        [&] ( const ComposedDomain & ) -> AbstractTypePtr {
            return ComposedType::make( type, dom );
        } ).value();
}

Type * liftTypeLLVM( Type * type, DomainPtr domain ) {
    if ( type->isVoidTy() )
        return type;
    auto lifted = liftType( type, domain );
    return lifted->pointer() ? lifted->llvm()->getPointerTo() : lifted->llvm();
}

Type * lowerTypeLLVM( Type * type ) {
    if ( !isAbstract( type ) )
        return type;
    auto lowered = getFromLLVM( type );
    return lowered->pointer() ? lowered->origin()->getPointerTo() : lowered->origin();
}

} // namespace abstract
} // namespace lart
