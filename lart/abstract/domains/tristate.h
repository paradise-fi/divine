// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct TristateDomain final : Common {

    TristateDomain() = default;

    llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & ) override;

    bool is( llvm::Type * type ) override {
        return type->isStructTy() && type->getStructName() == "lart.tristate";
    }

    llvm::Type * abstract( llvm::Type * ) override {
        UNSUPPORTED_BY_DOMAIN
    }

    DomainPtr domain() const override { return _domain; }

private:
    const DomainPtr _domain = Domain::make( DomainValue::Tristate );
};

} // namespace abstract
} // namespace lart
