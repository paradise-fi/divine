// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/tainting.h>

namespace lart::abstract {

    struct Synthesize {
        void run( llvm::Module & m );
        void dispach( const Taint & taint );
    };

} // namespace lart::abstract
