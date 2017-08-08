// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/types/common.h>
#include <lart/abstract/types/composed.h>
#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

template< typename T >
using Mapper = std::map< T, T>;

using ValueMap = Mapper< llvm::Value * >;
using FunctionMap = Mapper< llvm::Function * >;

struct Substituter {
    using Abstraction = std::shared_ptr< Common >;
    using Abstractions = std::map< DomainValue, Abstraction >;

    Substituter();

    /* Substitute abstract value */
    void process( llvm::Value * value );

    /* Substitute all abstract values in function 'fn' */
    void process( llvm::Function * fn );

    /* Returns type in corresponding abstract domain if the type is
     * 'lart' type. Else it returns original type */
    llvm::Type * process( llvm::Type * ) const;
    llvm::Type * process( const AbstractTypePtr & type ) const;

    void clean( llvm::Function * );
    void clean( llvm::Module & );

    /* Returns true if the function was already proccessed */
    bool visited( llvm::Function * ) const;

    /* Stores function f2 as substituted equivalent for function f1 */
    void store( llvm::Function * f1, llvm::Function * f2 );

    /* Add 'abstraction' to available abstractions for builder */
    void registerAbstraction( Abstraction && abstraction  );

    void changeCallFunction( llvm::CallInst * );
private:
    void substituteAbstractIntrinsic( llvm::CallInst * );
    void substitutePhi( llvm::PHINode * );
    void substituteSelect( llvm::SelectInst * );
    void substituteGEP( llvm::GetElementPtrInst * );
    void substituteBranch( llvm::BranchInst * );
    void substituteCall( llvm::CallInst * );
    void substituteCast( llvm::CastInst * );
    void substituteReturn( llvm::ReturnInst * );

    llvm::Type * createStructType( const ComposedTypePtr & ct );

    const Abstraction getAbstraction( llvm::Value * value ) const;
    const Abstraction getAbstraction( const AbstractTypePtr & ty ) const;

    bool isSubstituted( llvm::Value * value ) const;
    bool isSubstituted( llvm::Type * type ) const;

    ValueMap processedValues;
    FunctionMap processedFunctions;

    Abstractions abstractions;
};

} // namespace abstract
} // namespace lart
