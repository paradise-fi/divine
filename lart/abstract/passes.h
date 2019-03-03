// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>

#include <lart/abstract/vpa.h>
#include <lart/abstract/decast.h>
#include <lart/abstract/syntactic.h>
#include <lart/abstract/branching.h>
#include <lart/abstract/stash.h>
#include <lart/abstract/content.h>
#include <lart/abstract/assume.h>
#include <lart/abstract/bcp.h>
#include <lart/abstract/concretization.h>
#include <lart/abstract/tainting.h>
#include <lart/abstract/synthesize.h>
#include <lart/abstract/interrupt.h>
#include <lart/abstract/cleanup.h>

namespace lart::abstract {

    using SubstitutionPass = ChainedPass< Concretization, Tainting, Synthesize >;

    struct PassWrapper {
        static PassMeta meta() {
            return passMeta< PassWrapper >(
                "Abstraction", "Abstract annotated values to given domains." );
        }

        void run( llvm::Module & m ) {
            auto passes = make_chained_pass( CreateAbstractMetadata()
                                           , VPA()
                                           , Decast()
                                           , VPA() // run once more to propagate through decasted functions
                                           , StashingPass()
                                           , Syntactic()
                                           , ContentPass()
                                           , LowerToBool()
                                           , AddAssumes()
                                           , SubstitutionPass()
                                           , CallInterrupt()
                                           // , Cleanup()
                                           );
            passes.run( m );
        }

    };

    inline std::vector< PassMeta > passes() {
        return { PassWrapper::meta() };
    }

} // namespace lart::abstract

