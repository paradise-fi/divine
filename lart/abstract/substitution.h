// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>

#include <lart/abstract/concretization.h>
#include <lart/abstract/tainting.h>

namespace lart::abstract {

    struct Synthesize {
        void run( llvm::Module& );
        void process( llvm::CallInst* );
    };

    using SubstitutionPass = ChainedPass< Concretization, Tainting, Synthesize >;

} // namespace lart::abstract

