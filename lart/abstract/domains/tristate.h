// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct TristateDomain final : Common {

    TristateDomain() = default;

    llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & ) override;

    bool is( llvm::Type * ) override {
        return false;
    }

    llvm::Type * abstract( llvm::Type * ) override {
        UNSUPPORTED_BY_DOMAIN
    }

    Domain::Value domain() const override {
        return Domain::Value::Tristate;
    }
};

} // namespace abstract
} // namespace lart
