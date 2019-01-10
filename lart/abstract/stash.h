// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_set>

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct Stash {
    void run( llvm::Module& );

private:
    void ret_stash( llvm::CallInst*, llvm::Function* );
    void ret_unstash( llvm::CallInst* );
    void arg_unstash( llvm::CallInst*, llvm::Function* );
    void arg_stash( llvm::CallInst* );

    std::unordered_set< llvm::Function* > stashed;
};

} // namespace abstract
} // namespace lart
