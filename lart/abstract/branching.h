
// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct ExpandBranching {
    void run( llvm::Module& );
    void expand_to_i1( llvm::Instruction* );
};

} // namespace abstract
} // namespace lart

