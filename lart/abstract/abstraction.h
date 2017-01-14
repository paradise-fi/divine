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

    void init( llvm::Module & );

    void preprocess( llvm::Function * );

    void process( llvm::Instruction * );
    void process( llvm::Argument * );
    void process( llvm::Function *, std::vector< llvm::Value * > const& );

    void processAllocas( llvm::Function * );

    void propagate( llvm::Value * );
    void propagateFromCall( llvm::CallInst * );
    llvm::Function * changeReturn( llvm::Function * );

    AbstractBuilder builder;
    AbstractWalker walker;

    std::set< llvm::Function * > _unused;
};

PassMeta abstraction_pass() {
    return Abstraction::meta();
}

} // namespace abstract
} // namespace lart
