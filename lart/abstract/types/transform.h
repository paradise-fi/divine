// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/types/common.h>
#include <lart/abstract/types/scalar.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

using AbstractTypePtr = std::shared_ptr< AbstractType >;

// Lift 'type' to abstract type in 'domain'
AbstractTypePtr liftType( llvm::Type * type, const Domain::Value & domain );

AbstractTypePtr liftType( llvm::Type * type, const DomainMap & dmap );

// Lift 'type' to abstract 'domain' and return llvm representation of it
llvm::Type * liftTypeLLVM( llvm::Type * type, const Domain::Value & domain );

llvm::Type * lowerTypeLLVM( llvm::Type * type );

} // namespace abstract
} // namespace lart
