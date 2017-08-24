// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Symbolic final : Common {

    Symbolic( llvm::Module & m, TMap & tmap ) : Common( tmap ) {
        formula_type = m.getFunction( "__abstract_sym_load" )->getReturnType();
    }

    llvm::Value * process( llvm::CallInst *, Values & ) override;

    bool is( llvm::Type * ) override;

    llvm::Type * abstract( llvm::Type * ) override;

    Domain domain() const override { return Domain::Symbolic; }

    static bool isPresent( llvm::Module & m ) {
        return m.getFunction( "__abstract_sym_load" ) != nullptr;
    }

private:
    llvm::ConstantInt * bitwidth( llvm::Type * );
    Values arguments( llvm::CallInst *, Args & );

    llvm::Type * formula_type;
};

} // namespace abstract
} // namespace lart

