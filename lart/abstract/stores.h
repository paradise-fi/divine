// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct AddStores {
    void run( llvm::Module& );
    void process( llvm::StoreInst* );
};

} // namespace abstract
} // namespace lart
