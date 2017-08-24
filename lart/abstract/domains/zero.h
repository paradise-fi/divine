// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Zero final : Common {

    Zero( llvm::Module & m, TMap & tmap ) : Common( tmap ) {
        zero_type = m.getFunction( "__abstract_zero_load" )->getReturnType();
    }

    llvm::Value * process( llvm::CallInst *, Values & ) override;

    bool is( llvm::Type * ) override;

    llvm::Type * abstract( llvm::Type * ) override;

    Domain domain() const override { return Domain::Zero; }

    static bool isPresent( llvm::Module & m ) {
        return m.getFunction( "__abstract_zero_load" ) != nullptr;
    }

private:
    llvm::Type * zero_type;
};

} // namespace abstract
} // namespace lart
