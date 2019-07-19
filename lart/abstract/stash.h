// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/abstract/util.h>

namespace lart::abstract {

    struct StashPass {
        void run( llvm::Module & m );
    };

    struct UnstashPass {
        void run( llvm::Module & m );
    };

    constexpr char unpacked_argument[] = "lart.abstract.unpacked.argument.";

    auto unpacked_arguments( llvm::Module * m ) -> std::vector< llvm::CallInst * >;

    auto unstash( llvm::CallInst * call ) -> llvm::Value *;
    auto unstash( llvm::Function * fn ) -> llvm::Value *;

    auto stash( llvm::ReturnInst * ret, llvm::Value * val ) -> llvm::Value *;

} // namespace lart::abstract
