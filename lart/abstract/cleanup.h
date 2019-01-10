// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct Cleanup {
    void run( llvm::Module& );
};

} // namespace abstract
} // namespace lart
