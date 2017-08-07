// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/scalar.h>
#include <lart/abstract/types/common.h>

namespace lart {
namespace abstract {
namespace types {

namespace {

template< TypeBase::Value base >
auto MakeIntPtr = [] ( Type * type, Domain::Value domain ) {
    return std::make_shared< AbstractInt< base > >( type, domain );
};

using AbstractTypePtr = std::shared_ptr< AbstractType >;

static AbstractTypePtr liftAbstractInt( IntegerType * type, Domain::Value domain ) {
    switch ( type->getBitWidth() ) {
        case 1:
            return MakeIntPtr< TypeBase::Value::Int1 >( type, domain ) ;
        case 8:
            return MakeIntPtr< TypeBase::Value::Int8 >( type, domain ) ;
        case 16:
            return MakeIntPtr< TypeBase::Value::Int16 >( type, domain ) ;
        case 32:
            return MakeIntPtr< TypeBase::Value::Int32 >( type, domain ) ;
        case 64:
            return MakeIntPtr< TypeBase::Value::Int64 >( type, domain ) ;
    }
    UNREACHABLE( "Lisfting integer of unsupported bitwidth." );
}

} // empty namespace

static Type * lift( Type * type, Domain::Value domain ) {
    bool ptr = type->isPointerTy();
    type = stripPtr( type );
    bool abstract = isAbstract( type );

    Type * lifted;
    // Force lift to be idempotent
    auto st = llvm::dyn_cast< StructType >( type );
    if ( abstract && types::domain( st ).value() == domain ) {
        lifted = type;
    } else {
        type = abstract ? types::origin( st ) : type;
        llvmcase( type,
        [&]( llvm::IntegerType * t ) {
            lifted = liftAbstractInt( t, domain )->llvm();
        },
        [&]( llvm::StructType * ) {
            UNREACHABLE( "Lifting of structs is not yet implemented." );
        } );
    }
    return ptr ? lifted->getPointerTo() : lifted;
}

static Type * lower( Type * type ) {
    if ( !isAbstract( type ) )
        return type;
    bool ptr = type->isPointerTy();
    auto st = llvm::cast< llvm::StructType >( types::stripPtr( type ) );
    auto base = types::base( st ).value();
    auto lowered = TypeBase::llvm( base, type->getContext() );
    return ptr ? lowered->getPointerTo() : lowered;
}

} //namespace types
} // namespace abstract
} // namespace lart
