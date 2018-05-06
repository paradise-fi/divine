// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>
// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

#define UNSUPPORTED_BY_DOMAIN \
        UNREACHABLE( "This function is not supported by this domain.");

namespace lart {
namespace abstract {

struct Common {
    virtual ~Common() { }

    /*
     * Get the LLVM type that represents the abstract type corresponding to the
     * concrete type t. Must be idempotent, i.e. abstract( abstract( x ) ) ==
     * abstract( x ) for all x. The result must either be "t" itself, or it
     * must be an abstract type (see isAbstract).
     */
    // virtual llvm::Type *abstract( llvm::Type *t ) = 0;

    /*
     * This particular abstraction deals with types "%lart.domain.*" --
     * i.e. if domain is "interval" then values of type
     * "%lart.interval.*" are the responsibility of this abstraction.
     */
    virtual Domain domain() const = 0;

    /* The outside interface of abstraction passes.
     * Translate an abstract intrinsic call to a code block computing the effect of the
     * abstract equivalent.
     * Returns resulting value of abstracted instruction.
     */
    virtual llvm::Value* process( llvm::Instruction *i, Values &args  ) = 0;

    virtual llvm::Value* lift( llvm::Value *v ) = 0;

    virtual llvm::Type *type( llvm::Module *m, llvm::Type *type ) const = 0;

    /* Takes type in the domain and returns default value of it. */
    virtual llvm::Value *default_value( llvm::Type *type ) const = 0;
};

}
}
