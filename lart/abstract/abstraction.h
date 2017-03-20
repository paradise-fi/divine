// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/trivial.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>

#include <lart/abstract/builder.h>
#include <lart/abstract/walker.h>

namespace lart {
namespace abstract {

struct Abstraction : lart::Pass
{
    virtual ~Abstraction() {}

    static PassMeta meta() {
        return passMeta< Abstraction >(
            "Abstraction", "Substitutes annotated values by abstract values." );
    }

    llvm::PreservedAnalyses run( llvm::Module & ) override;

private:
    AbstractBuilder builder;
};

static inline PassMeta abstraction_pass() {
    return Abstraction::meta();
}

} // namespace abstract
} // namespace lart
