// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

//FIXME refactor annotation definitions out
#include <lart/abstract/walkergraph.h>
#include <lart/abstract/domains/domains.h>
#include <lart/support/util.h>

namespace lart {
namespace abstract {

struct AbstractBuilder {

    llvm::Value * process( llvm::Value * );
    llvm::Value * process( const FunctionNodePtr & );

    void clone( const FunctionNodePtr & );

    void clean( std::vector< llvm::Value * > & );

private:
    /* maps real types to abstract types */
    void store( llvm::Value *, llvm::Value * );
    void store( llvm::Function *, llvm::Function * );


    std::vector< llvm::Type * > argTypes( llvm::CallInst * );

    llvm::Value * create( llvm::Instruction * );
    llvm::Value * createPtrInst( llvm::Instruction * );
    llvm::Value * createInst( llvm::Instruction * );

    bool ignore( llvm::Instruction * );

    llvm::Value * createAlloca( llvm::AllocaInst * );
    llvm::Value * createLoad( llvm::LoadInst * );
    llvm::Value * createStore( llvm::StoreInst * );
    llvm::Value * createICmp( llvm::ICmpInst * );
    llvm::Value * createBranch( llvm::BranchInst * );
    llvm::Value * createBinaryOperator( llvm::BinaryOperator * );
    llvm::Value * createCast( llvm::CastInst * );
    llvm::Value * createPhi( llvm::PHINode * );
    llvm::Value * createCall( llvm::CallInst * );
    llvm::Value * createReturn( llvm::ReturnInst * );

    llvm::Value * createPtrCast( llvm::CastInst * );
    llvm::Value * createGEP( llvm::GetElementPtrInst * );

    llvm::Value * lower( llvm::Value *, llvm::IRBuilder<> & );
    llvm::Value * lift( llvm::Value *, llvm::IRBuilder<> & );
    llvm::Value * toTristate( llvm::Value * v, Domain::Value domain, llvm::IRBuilder<> & irb );

    llvm::Value * clone( llvm::CallInst * );

    llvm::Value * processLiftCall( llvm::CallInst * );
    llvm::Value * processIntrinsic( llvm::CallInst * );
    llvm::Value * processCall( llvm::CallInst * );

    llvm::Function * getStoredFn( llvm::Function *,
                                  llvm::ArrayRef< llvm::Type * > );

    llvm::Value * _process( llvm::Instruction * );
    llvm::Value * _process( llvm::Argument * );

    //std::map< llvm::Type *, llvm::Type * > _types;
    std::map< llvm::Value *, llvm::Value * > _values;
    std::map< llvm::Function *, std::vector< llvm::Function * > > _functions;
};

} // namespace abstract
} // namespace lart
