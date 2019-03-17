// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_set>

#include <lart/support/pass.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct Stash {
    void run( llvm::Module& );
private:
    void process_return_value( llvm::CallInst*, llvm::Function* );

    std::unordered_set< llvm::Function* > stashed;
};

struct Unstash {
    void run( llvm::Module& );
private:
    void process_arguments( llvm::CallInst*, llvm::Function* );

    std::unordered_set< llvm::Function* > unstashed;
};

using StashingPass = ChainedPass< Unstash, Stash >;

} // namespace abstract
} // namespace lart
