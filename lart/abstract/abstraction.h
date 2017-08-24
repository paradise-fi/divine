// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/builder.h>
#include <lart/abstract/data.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct Abstraction {
    Abstraction( PassData & data ) : data( data ) {}

    void run( llvm::Module & );

    PassData & data;
};

} // namespace abstract
} // namespace lart
