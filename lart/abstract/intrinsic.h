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
namespace intrinsic {

using MaybeDomain = Domain::MaybeDomain;
MaybeDomain domain( const llvm::CallInst * );
MaybeDomain domain( const llvm::Function * );
const std::string name( const llvm::CallInst * );
const std::string name( const llvm::Function * );
const std::string ty1( const llvm::CallInst * );
const std::string ty1( const llvm::Function * );
const std::string ty2( const llvm::CallInst * );
const std::string ty2( const llvm::Function * );

const std::string tag( const AbstractValue & );
const std::string tag( const llvm::Instruction * , Domain::Value );

auto types( std::vector< llvm::Value * > & ) -> llvm::ArrayRef< llvm::Type * >;

llvm::Function * get( llvm::Module *,
                      llvm::Type *,
                      const std::string &,
                      llvm::ArrayRef < llvm::Type * > );

llvm::CallInst * build( llvm::Module *,
                        llvm::IRBuilder<> &,
                        llvm::Type *,
                        const std::string &,
                        std::vector< llvm::Value * > args = {} );

//helpers
bool is( const llvm::Function * );
bool is( const llvm::CallInst * );
bool isAssume( const llvm::CallInst * );
bool isLift( const llvm::CallInst * );
bool isLower( const llvm::CallInst * );

} // namespace intrinsic
} // namespace abstract
} // namespace lart
