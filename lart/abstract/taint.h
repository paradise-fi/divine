// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

#include <unordered_set>

namespace lart {
namespace abstract {

struct Tainting {
    void run( llvm::Module& );

    void taint( llvm::Instruction* );
private:
    std::unordered_set< llvm::Value* > tainted;
};

struct TaintBranching {
    void run( llvm::Module& );
    void expand( llvm::Value*, llvm::BranchInst* );
};

struct LifterSynthesize {
    void run( llvm::Module& );
    void process( llvm::CallInst* );
    void process_assume( llvm::CallInst* );
};

// Creates function intrinsic with given return type and argument types.
llvm::Function* get_taint_fn( llvm::Module*, llvm::Type *ret, Types args );

// Creates call to 'vm.test.taint' function.
//
// Checks whether given instruction is tainted.
//
// If function does not exist, it is created using 'get_taint_fn'.
llvm::Instruction* create_taint( llvm::Instruction*, const Values &args );

// Checks whether value is taintable.
// We consider taintable only binary operations, cmp instructions,
// trunc, zext and sext instruction.
bool is_taintable( llvm::Value * );

} // namespace abstract
} // namespace lart
