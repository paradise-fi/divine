// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Tristate final : Common {
    llvm::Value * process( llvm::CallInst *, Values & ) override;
    Domain domain() const override { return Domain::Tristate; }
};

} // namespace abstract
} // namespace lart
