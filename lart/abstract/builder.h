// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>

namespace lart {
namespace abstract {

struct AbstractBuilder {

    /* maps real types to abstract types */
    void store( llvm::Type *, llvm::Type * );
    void store( llvm::Value *, llvm::Value * );
    void store( llvm::Function *, llvm::Function * );

    llvm::Value * process( llvm::Instruction * );
    llvm::Value * process( llvm::Argument * );
    llvm::Value * process( llvm::CallInst *, llvm::Function * );

    void clean( std::vector< llvm::Value * > & );

    void annotate();

	llvm::Function * changeReturn( llvm::Function * );

    std::vector< llvm::Type * > arg_types( llvm::CallInst * );

    llvm::Value * create( llvm::Instruction * );
    bool ignore( llvm::Instruction * );

    llvm::Value * createAlloca( llvm::AllocaInst * );
    llvm::Value * createLoad( llvm::LoadInst * );
    llvm::Value * createStore( llvm::StoreInst * );
    llvm::Value * createICmp( llvm::ICmpInst * );
    llvm::Value * createSelect( llvm::SelectInst * );
    llvm::Value * createBranch( llvm::BranchInst * );
    llvm::Value * createBinaryOperator( llvm::BinaryOperator * );
    llvm::Value * createCast( llvm::CastInst * );
    llvm::Value * createPhi( llvm::PHINode * );
    llvm::Value * createCall( llvm::CallInst * );
    llvm::Value * createReturn( llvm::ReturnInst * );

    llvm::Value * lower( llvm::Value *, llvm::IRBuilder<> & );
    llvm::Value * lift( llvm::Value *, llvm::IRBuilder<> & );
    llvm::Value * toTristate( llvm::Value * v, llvm::IRBuilder<> & );

    llvm::Value * clone( llvm::CallInst * );

    llvm::Value * processLiftCall( llvm::CallInst * );
    llvm::Value * processIntrinsic( llvm::CallInst * );
	llvm::Value * processAnonymous( llvm::CallInst * );
    llvm::Value * processCall( llvm::CallInst * );

    void annotate( llvm::Function *, const std::string & );

    llvm::Function * getStoredFn( llvm::Function *,
                                  llvm::ArrayRef< llvm::Type * > );

    bool needToPropagate( llvm::CallInst * );

    std::map< llvm::Type *, llvm::Type * > _types;
    std::map< llvm::Value *, llvm::Value * > _values;
    std::map< llvm::Function *, std::vector< llvm::Function * > > _functions;

    std::map< llvm::Function *, std::string > _anonymous;

	std::set< llvm::Function * > _unusedFunctions;
    std::set< llvm::Instruction * > _unusedLifts;
};

} // namespace abstract
} // namespace lart
