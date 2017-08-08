// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

//FIXME refactor annotation definitions out
#include <lart/abstract/walker.h>
#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/composed.h>
#include <lart/support/util.h>

namespace lart {
namespace abstract {

struct AbstractBuilder {

    void process( const AbstractValue & );

    llvm::Function * process( const FunctionNodePtr & );
    void clone( const FunctionNodePtr & );
    void clean( std::vector< llvm::Value * > & );

private:
    /* maps real types to abstract types */
    void store( llvm::Value *, llvm::Value * );
    void store( llvm::Function *, llvm::Function * );


    std::vector< llvm::Type * > argTypes( llvm::CallInst * );

    llvm::Value * create( const AbstractValue & );

    bool ignore( llvm::Instruction * );

    llvm::Value * createAlloca( const AbstractValue & );
    llvm::Value * createLoad( const AbstractValue & );
    llvm::Value * createStore( const AbstractValue & );
    llvm::Value * createICmp( const AbstractValue & );
    llvm::Value * createBranch( const AbstractValue & );
    llvm::Value * createBinaryOperator( const AbstractValue & );
    llvm::Value * createCast( const AbstractValue & );
    llvm::Value * createPhi( const AbstractValue & );
    llvm::Value * createCall( const AbstractValue & );
    llvm::Value * createReturn( const AbstractValue & );

    llvm::Value * createPtrCast( const AbstractValue & );
    llvm::Value * createGEP( const AbstractValue & );

    llvm::Value * lower( llvm::Value *, llvm::IRBuilder<> & );
    llvm::Value * lift( llvm::Value *, Domain::Value , llvm::IRBuilder<> & );
    llvm::Value * toTristate( llvm::Value *, Domain::Value , llvm::IRBuilder<> & );

    llvm::Value * clone( llvm::CallInst * );

    llvm::Value * processLiftCall( llvm::CallInst * );
    llvm::Value * processIntrinsic( llvm::CallInst * );
    llvm::Value * processCall( const AbstractValue & );

    llvm::Function * getStoredFn( llvm::Function *,
                                  llvm::ArrayRef< llvm::Type * > );

    using Values = std::vector< llvm::Value * >;
    Values operands( const AbstractValue & );

    // TODO values to be map< Value, AbstractValue >
    std::map< llvm::Value *, llvm::Value * > _values;
    std::map< llvm::Function *, std::vector< llvm::Function * > > _functions;
};

} // namespace abstract
} // namespace lart
