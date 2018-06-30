#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

namespace lart
{
    // LowerSelect converts SelectInst instructions into conditional branch and PHI
    // instructions.
    struct LowerSelect
    {
        bool runOnFunction( llvm::Function &fn, bool changed = false );
        void lower( llvm::SelectInst *si );
    };
}
