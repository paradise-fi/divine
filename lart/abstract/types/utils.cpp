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
        return AbstractType::make< ScalarType >( type );
    else
        return AbstractType::make< ComposedType >( type );
}

AbstractTypePtr liftType( Type * type, DomainPtr dom ) {
    // Force lift to be idempotent
    if ( isAbstract( stripPtr( type ) ) )
        return getFromLLVM( type );
    return dom->match(
        [&] ( UnitDomain ) -> AbstractTypePtr {
            return AbstractType::make< ScalarType >( type, dom );
        },
        [&] ( ComposedDomain ) -> AbstractTypePtr {
            return AbstractType::make< ComposedType >( type, dom );
        } ).value();
}

Type * liftTypeLLVM( Type * type, DomainPtr domain ) {
    if ( type->isVoidTy() )
        return type;
    return liftType( type, domain )->llvm();
}

Type * lowerTypeLLVM( Type * type ) {
    if ( !isAbstract( type ) )
        return type;
    return getFromLLVM( type )->origin();
}

} // namespace abstract
} // namespace lart
