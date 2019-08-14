#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

#include <llvm/Pass.h>
DIVINE_UNRELAX_WARNINGS

namespace lart
{
    // LowerSelect converts SelectInst instructions into conditional branch and PHI
    // instructions.
    struct LowerSelectPass : llvm::FunctionPass
    {
        static char id;

        LowerSelectPass() : llvm::FunctionPass( id ) { }

        virtual bool runOnFunction( llvm::Function &fn ) override;
        void lower( llvm::SelectInst *si );
    };

    static llvm::FunctionPass * createLowerSelectPass() {
        return new LowerSelectPass();
    }
}
