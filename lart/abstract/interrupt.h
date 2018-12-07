// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct CallInterrupt {
    void run( llvm::Module &m );
};

} // namespace abstract
} // namespace lart
