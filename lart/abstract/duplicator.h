// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_set>

namespace lart {
namespace abstract {

struct Duplicator {
    void run( llvm::Module& );
    void process( llvm::Instruction* );
private:
    std::unordered_set< llvm::Function* > seen;
    std::map< llvm::Value*, llvm::Value* > dups;
};

} // namespace abstract
} // namespace lart

