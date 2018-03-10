// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_set>

namespace lart {
namespace abstract {

struct Tainting {
    void run( llvm::Module & );

    void taint( llvm::Instruction * );
private:
    std::unordered_set< llvm::Value* > tainted;
};

} // namespace abstract
} // namespace lart
