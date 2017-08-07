// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Symbolic final : Common {

    Symbolic( llvm::Module & m );

    llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & ) override;

    bool is( llvm::Type * ) override;

    llvm::Type * abstract( llvm::Type * ) override;

    Domain::Value domain() const override {
        return Domain::Value::Symbolic;
    }

    static bool isPresent( llvm::Module & m ) {
        return m.getFunction( "__abstract_sym_load" ) != nullptr;
    }

private:
    llvm::Type * formula_type;
};

} // namespace abstract
} // namespace lart

