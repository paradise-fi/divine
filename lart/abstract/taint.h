// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct Tainting {
    void run( llvm::Module & );
};

} // namespace abstract
} // namespace lart
