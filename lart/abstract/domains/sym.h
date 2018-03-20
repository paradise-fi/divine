// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Symbolic final : Common {
    llvm::Value * process( llvm::Instruction *i, Values &args ) override;
    Domain domain() const override { return Domain::Symbolic; }
};

} // namespace abstract
} // namespace lart

