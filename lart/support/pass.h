// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
DIVINE_UNRELAX_WARNINGS
#include <vector>

#ifndef LART_SUPPORT_PASS_H
#define LART_SUPPORT_PASS_H

namespace lart {

struct Pass
{
    virtual llvm::PreservedAnalyses run( llvm::Module &m ) = 0;
    virtual llvm::PreservedAnalyses run( llvm::Module *m ) {
        return run( *m );
    }
    virtual ~Pass() { };
    static std::string name() { return "anonymous LART pass"; }
};

}

#endif
