// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/value.h>

namespace lart {
namespace abstract {

struct Intrinsic {
    using Args = std::vector< llvm::Value * >;
    using ArgTypes = std::vector< llvm::Type * >;

    enum class Type {
        LLVM, Lift, Lower, Assume
    };

    Intrinsic( llvm::Module * m,
               llvm::Type * rty,
               const std::string & tag,
               const ArgTypes & args = {} );

    Intrinsic( llvm::Function * intr ) :
        intr( intr ) {}

    Intrinsic( llvm::CallInst * call ) :
        intr( call->getCalledFunction() ) {}

    DomainPtr domain() const;
    std::string name() const;
    Type type() const;
    bool is() const;
    llvm::Function * declaration() const { return intr; }
    std::string nameElement( size_t idx ) const; //TODO rename to elementName

    template< size_t idx >
    llvm::Type * argType() const {
        assert( intr->arg_size() > idx );
        return intr->getFunctionType()->getParamType( idx );
    }


private:
    llvm::Function * intr;
};

static bool isIntrinsic( llvm::Function * fn ) {
    return Intrinsic( fn ).is();
}

static bool isIntrinsic( llvm::CallInst * call ) {
    return isIntrinsic( call->getCalledFunction() );
}

static bool isAssume( const Intrinsic & intr ) {
    return intr.type() == Intrinsic::Type::Assume;
}

static bool isAssume( llvm::CallInst * call ) {
    return isAssume( Intrinsic( call ) );
}

static bool isLift( const Intrinsic & intr ) {
    return intr.type() == Intrinsic::Type::Lift;
}

static bool isLift( llvm::CallInst* call ) {
    return isLift( Intrinsic( call ) );
}

static bool isLower( const Intrinsic & intr ) {
    return intr.type() == Intrinsic::Type::Lower;
}

static bool isLower( llvm::Function * call ) {
    return isLower( Intrinsic( call ) );
}

// Returns name for abstract value as "lart.<domain>.<inst name>.<extra data>"
static std::string intrinsicName( const AbstractValue & av ) {
    auto i = llvm::cast< llvm::Instruction >( av.value() );
    auto res = "lart." + av.domain()->name() + "." + i->getOpcodeName();

    if ( llvm::isa< llvm::AllocaInst >( av.value() ) )
        return res + "." + av.type()->baseName();
    if ( llvm::isa< llvm::LoadInst >( av.value() ) )
        return res + "." + av.type()->baseName();
    if ( llvm::isa< llvm::StoreInst >( av.value() ) )
        return res;
    if ( llvm::isa< llvm::ICmpInst >( av.value() ) )
        return res;
    if ( llvm::isa< llvm::BinaryOperator >( av.value() ) )
        return res + "." + av.type()->baseName();
    if ( auto i = llvm::dyn_cast< llvm::CastInst >( av.value() ) )
        return res + "." + llvmname( i->getSrcTy() ) + "." + llvmname( i->getDestTy() );

    UNREACHABLE( "Unhandled intrinsic." );
}

} // namespace abstract
} // namespace lart
