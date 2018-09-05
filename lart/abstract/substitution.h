// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>

#include <map>

namespace lart {
namespace abstract {

struct InDomainDuplicate {
    void run( llvm::Module& );
    void process( llvm::Instruction* );
};


struct Tainting {
    void run( llvm::Module& );
    llvm::Value* process( llvm::Instruction* );
};


struct FreezeStores {
    void run( llvm::Module& );
    void process( llvm::StoreInst* );
};


struct Synthesize {
    void run( llvm::Module& );
    void process( llvm::CallInst* );
};

} // namespace abstract
} // namespace lart

