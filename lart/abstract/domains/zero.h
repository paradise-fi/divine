// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Zero final : Common {
    llvm::Value * process( llvm::CallInst *, Values & ) override;
    Domain domain() const override { return Domain::Zero; }
};

} // namespace abstract
} // namespace lart
