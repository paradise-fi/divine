// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/value.h>
#include <lart/abstract/util.h>

#include <sstream>

namespace lart {
namespace abstract {

namespace intrinsic {

struct IntrinsicWrapper {
    IntrinsicWrapper( std::string name, Domain dom )
        : name( name ), domain( dom ) {}
    bool isLift() const { return name == "lift"; }
    bool isLower() const { return name == "lower"; }
    bool isAssume() const { return name == "assume"; }
    bool isAlloca() const { return name == "alloca"; }

    std::string name;
    Domain domain;
};

using Token = std::string;
using Tokens = std::vector< Token >;
static Tokens parse( const std::string & name ) {
    std::istringstream ss( name );
    Token token; Tokens tokens;
    while( std::getline( ss, token, '.' ) )
        tokens.push_back( token );
    return tokens;
}

static Maybe< IntrinsicWrapper > parse( llvm::Function * fn ) {
    if ( !fn || !fn->hasName() )
        return Maybe< IntrinsicWrapper >::Nothing();
    auto tokens = parse( fn->getName() );
    if ( tokens.size() > 2 && tokens[ 0 ] == "lart" ) {
        Domain dom = DomainTable[ tokens[ 1 ] ];
        std::string name = tokens[ 2 ];
        return Maybe< IntrinsicWrapper >::Just( IntrinsicWrapper( name, dom ) );
    }
    return Maybe< IntrinsicWrapper >::Nothing();
}

} // anonymous namespace

static bool isIntrinsic( llvm::Function * fn ) {
    return intrinsic::parse( fn ).isJust();
}

static bool isIntrinsic( llvm::Instruction * i ) {
    if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
        return isIntrinsic( call->getCalledFunction() );
    return false;
}

static bool isAssume( llvm::Function * fn ) {
    auto intr = intrinsic::parse( fn );
    if ( intr.isJust() )
        return intr.value().isAssume();
    return false;
}

static bool isAssume( llvm::Instruction * i ) {
    if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
        return isAssume( call->getCalledFunction() );
    return false;
}

static bool isLift( llvm::Function * fn ) {
    auto intr = intrinsic::parse( fn );
    if ( intr.isJust() )
        return intr.value().isLift();
    return false;
}

static bool isLift( llvm::Instruction * i ) {
    if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
        return isLift( call->getCalledFunction() );
    return false;
}

static bool isLower( llvm::Function * fn ) {
    auto intr = intrinsic::parse( fn );
    if ( intr.isJust() )
        return intr.value().isLower();
    return false;
}

static bool isLower( llvm::Instruction * i ) {
    if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
        return isLower( call->getCalledFunction() );
    return false;
}

static bool isAlloca( llvm::Function * fn ) {
    auto intr = intrinsic::parse( fn );
    if ( intr.isJust() )
        return intr.value().isAlloca();
    return false;
}

static bool isAlloca( llvm::Instruction * i ) {
    if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
        return isAlloca( call->getCalledFunction() );
    return false;
}

static Domain domain( llvm::Function * fn ) {
    assert( isIntrinsic( fn ) );
    return intrinsic::parse( fn ).value().domain;
}

static Domain domain( llvm::CallInst * call ) {
    return domain( call->getCalledFunction() );
}

static Domain domain( llvm::Type * type ) {
    auto tokens = intrinsic::parse( llvmname( type ) );
    return DomainTable[ tokens[ 1 ] ];
}

using IntrinsicCalls = std::vector< llvm::CallInst * >;

template< typename Vs >
static IntrinsicCalls intrinsics( const Vs & vs ) {
    return query::query( vs ).filter( [] ( const auto & i ) {
        return isIntrinsic( i );
    } ).freeze();
}

static IntrinsicCalls intrinsics( llvm::Module * m ) {
    return intrinsics( llvmFilter< llvm::CallInst >( m ) );
}

} // namespace abstract
} // namespace lart
