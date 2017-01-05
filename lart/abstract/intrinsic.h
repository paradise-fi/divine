// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {
namespace intrinsic {

enum Domain { Abstract, Tristate };
static std::string domainName( Domain d ) {
    switch( d ) {
        case Domain::Abstract : return "abstract";
        case Domain::Tristate : return "tristate";
    }
}

const std::string domain( const llvm::CallInst * );
const std::string domain( const llvm::Function * );
const std::string name( const llvm::CallInst * );
const std::string name( const llvm::Function * );
const std::string ty1( const llvm::CallInst * );
const std::string ty1( const llvm::Function * );
const std::string ty2( const llvm::CallInst * );
const std::string ty2( const llvm::Function * );

const std::string tag( const llvm::Instruction * );
const std::string tag( const llvm::Instruction * , Domain );

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

llvm::CallInst * anonymous( llvm::Module *,
                            llvm::IRBuilder<> &,
                            llvm::Type *,
                            std::vector< llvm::Value * > args = {} );

//helpers
bool is( const llvm::Function * );
bool is( const llvm::CallInst * );
bool isLift( const llvm::CallInst * );
bool isLower( const llvm::CallInst * );

} // namespace intrinsic
} // namespace abstract
} // namespace lart
