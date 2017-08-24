// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

static inline std::string llvmname( const llvm::Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

static inline llvm::Type * stripPtr( llvm::Type * type ) {
    return type->isPointerTy() ? type->getPointerElementType() : type;
}

template< typename TMap >
static bool isAbstract( llvm::Type * type, TMap & tmap ) {
    type = stripPtr( type );
    return tmap.safeOrigin( type ).isJust();
}

namespace {
static inline std::string typeName( llvm::Type * type, Domain dom ) {
    assert( type->isIntegerTy() );
    if ( dom == Domain::Tristate )
        return "lart." + DomainTable[ dom ];
    else
        return "lart." + DomainTable[ dom ] + "." + llvmname( type );
}

static inline llvm::Type * create( llvm::Type * type, Domain dom ) {
    auto name = typeName( type, dom );
    assert( !type->getContext().pImpl->NamedStructTypes.lookup( name ) );
    return ( dom == Domain::LLVM ) ? type : llvm::StructType::create( { type }, name );
}

template< typename TMap >
static llvm::Type * _lift( llvm::Type * type, Domain dom, TMap & tmap ) {
    assert( !type->isPointerTy() );
    if ( type->isVoidTy() )
        return type;
    if ( isAbstract( type, tmap ) )
        return type;
    auto t = tmap.safeLift( { type, dom } );
    if ( t.isJust() )
        return t.value();
    auto l = create( type, dom );
    tmap.insert( { type, dom }, l );
    return l;
}

} // anonymous namespace

template< typename TMap >
static llvm::Type * liftType( llvm::Type * type, Domain dom, TMap & tmap ) {
    if ( dom == Domain::LLVM )
        return type;
    auto res = _lift( stripPtr( type ), dom, tmap );
    return type->isPointerTy() ? res->getPointerTo() : res;
}

static llvm::Type * liftType( llvm::Type * type, Domain dom ) {
    if ( dom == Domain::LLVM )
        return type;
    auto name = typeName( type, dom );
    auto res = type->getContext().pImpl->NamedStructTypes.lookup( name );
    if ( type->isPointerTy() )
        return res->getPointerTo();
    else
        return res;
}


static inline llvm::Type * VoidType( llvm::LLVMContext & ctx ) {
   return llvm::Type::getVoidTy( ctx );
}

static inline llvm::Type * BoolType( llvm::LLVMContext & ctx ) {
    return llvm::IntegerType::get( ctx, 1 );
}

} // namespace abstract
} // namespace lart
