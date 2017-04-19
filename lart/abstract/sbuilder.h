// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/common.h>
namespace lart {
namespace abstract {

struct SubstitutionBuilder {
    SubstitutionBuilder() = default;
    SubstitutionBuilder( std::unique_ptr< Common > abstraction )
          : abstraction( std::move(abstraction) ) {}
    SubstitutionBuilder( SubstitutionBuilder && sb )
          : abstraction( std::move(sb.abstraction) ) {}

    ~SubstitutionBuilder() = default;

    SubstitutionBuilder& operator= ( SubstitutionBuilder && sb ) {
        abstraction = std::move( sb.abstraction );
        return *this;
    }

    void store( llvm::Value *, llvm::Value * );
    void store( llvm::Function *, llvm::Function * );

    void process( llvm::Instruction * );
    void process( llvm::Argument * );
    llvm::Function * process( llvm::Function * );

    void clean( llvm::Function * );
    void clean( llvm::Module & );

    bool stored( llvm::Function * ) const;

    void changeCallFunction( llvm::CallInst * );

    void substitutePhi( llvm::PHINode * );
    void substituteSelect( llvm::SelectInst * );
    void substituteBranch( llvm::BranchInst * );
    void substituteCall( llvm::CallInst * );
    void substituteCast( llvm::CastInst * );
    void substituteReturn( llvm::ReturnInst * );

    void processCall( llvm::CallInst * );

    std::map< llvm::Value *, llvm::Value * > _values;
    std::map< llvm::Function *, llvm::Function * > _functions;

    std::unique_ptr< Common > abstraction;
};

} // namespace abstract
} // namespace lart
