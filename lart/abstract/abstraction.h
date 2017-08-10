// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/builder.h>

namespace lart {
namespace abstract {

struct Abstraction {
    void run( llvm::Module & );

    AbstractBuilder builder;
};

} // namespace abstract
} // namespace lart
