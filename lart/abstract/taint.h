// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

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

} // namespace abstract
} // namespace lart
