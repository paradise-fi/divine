// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

namespace lart::abstract::meta {
    namespace tag {
        constexpr char abstract[] = "lart.abstract";
        constexpr char roots[] = "lart.abstract.roots";

        namespace transform {
            constexpr char prefix[] = "lart.transform";

            namespace ignore {
                constexpr char ret[] = "lart.transform.ignore.ret";
                constexpr char arg[] = "lart.transform.ignore.arg";
            } // namespace ignore

            constexpr char forbidden[] = "lart.transform.forbidden";
        } // namespace transform

        constexpr char domains[]  = "lart.abstract.domains";

        namespace domain {
            constexpr char name[] = "lart.abstract.domain.name";
            constexpr char kind[] = "lart.abstract.domain.kind";
        }
    } // namespace tag

    static inline bool ignore_call_of_function( llvm::Function * fn ) {
        return fn->getMetadata( tag::transform::ignore::arg );
    }

    static inline bool ignore_return_of_function( llvm::Function * fn ) {
        return fn->getMetadata( tag::transform::ignore::ret );
    }

    static inline bool is_forbidden_function( llvm::Function * fn ) {
        return fn->getMetadata( tag::transform::forbidden );
    }

} // namespace lart::abstract::meta
