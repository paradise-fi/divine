// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct TristateDomain final : Common {

    TristateDomain( TMap & tmap ) : Common( tmap ) {}

    llvm::Value * process( llvm::CallInst *, Values & ) override;

    bool is( llvm::Type * type ) override {
        return type->isStructTy() && type->getStructName() == "lart.tristate";
    }

    llvm::Type * abstract( llvm::Type * ) override {
        UNSUPPORTED_BY_DOMAIN
    }

    Domain domain() const override { return Domain::Tristate; }
};

} // namespace abstract
} // namespace lart
