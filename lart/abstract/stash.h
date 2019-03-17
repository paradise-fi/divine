// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct Stash {
    void run( llvm::Module& );
};

struct Unstash {
    void run( llvm::Module& );
private:
    void process_arguments( llvm::CallInst*, llvm::Function* );
};

using StashingPass = ChainedPass< Unstash, Stash >;

} // namespace abstract
} // namespace lart
