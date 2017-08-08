// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/composed.h>
#include <lart/abstract/types/scalar.h>
#include <lart/abstract/types/transform.h>

namespace lart {
namespace abstract {

AbstractTypePtr liftType( Type * type, const Domain::Value & domain ) {
    assert( isAbstract( type ) || type->isSingleValueType() );
    DomainMap dmap = { { 0, domain } };
    return liftType( type, dmap );
}

AbstractTypePtr liftType( Type * type, const DomainMap & dmap ) {
    type = stripPtr( type );
    bool abstract = isAbstract( type );

    // Force lift to be idempotent
    auto st = llvm::dyn_cast< StructType >( type );

    // TODO struct domain
    if ( abstract && TypeName( st ).domain().value() == dmap.at(0) ) {
        // TODO optimize
        return liftType( lowerTypeLLVM( type ), dmap );
    } else {
        if ( abstract ) {
            auto base = TypeName( st ).base().value();
            type = TypeBase::llvm( base, type->getContext() );
        }
        if ( auto t = llvm::dyn_cast< llvm::IntegerType >( type ) )
            return std::make_shared< ScalarType >( t, dmap.at(0) );
        if ( auto t = llvm::dyn_cast< llvm::StructType >( type ) )
            return std::make_shared< ComposedType >( t, dmap );
    }

    UNREACHABLE( "Can't lift type." );
}

Type * liftTypeLLVM( Type * type, const Domain::Value & domain ) {
    if ( type->isVoidTy() )
        return type;
    // TODO handle pointer types more generally
    bool ptr = type->isPointerTy();
    auto lifted = liftType( type, domain )->llvm();
    if ( ptr )
        return lifted->getPointerTo();
    else
        return lifted;
}

Type * lowerTypeLLVM( Type * type ) {
    if ( !isAbstract( type ) )
        return type;
    bool ptr = type->isPointerTy();
    auto st = llvm::cast< llvm::StructType >( stripPtr( type ) );
    auto base = TypeName( st ).base().value();
    auto lowered = TypeBase::llvm( base, type->getContext() );
    return ptr ? lowered->getPointerTo() : lowered;
}

} // namespace abstract
} // namespace lart
