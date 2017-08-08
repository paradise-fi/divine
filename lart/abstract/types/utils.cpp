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

AbstractTypePtr getFromLLVM( Type * type ) {
    auto sty = llvm::cast< llvm::StructType >( stripPtr( type ) );
    if ( TypeName( sty ).base() != TypeBaseValue::Struct )
        return ScalarType::make( type );
    else
        return ComposedType::make( type );
}

AbstractTypePtr liftType( Type * type, DomainPtr dom ) {
    // Force lift to be idempotent
    if ( isAbstract( stripPtr( type ) ) )
        return getFromLLVM( type );
    return dom->match(
        [&] ( UnitDomain ) -> AbstractTypePtr {
            return ScalarType::make( type, dom );
        },
        [&] ( ComposedDomain ) -> AbstractTypePtr {
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
    return getFromLLVM( type )->origin();
}

} // namespace abstract
} // namespace lart
