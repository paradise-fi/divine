// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <vector>

#ifndef LART_SUPPORT_PASS_H
#define LART_SUPPORT_PASS_H

namespace lart {

struct Pass
{
    virtual llvm::PreservedAnalyses run( llvm::Module &m ) = 0;
    static std::string name() { return "anonymous LART pass"; }
};

}

#endif
