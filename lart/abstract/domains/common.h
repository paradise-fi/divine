// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <set>

#define UNSUPPORTED_BY_DOMAIN \
        UNREACHABLE( "This function is not supported by this domain");

namespace lart {
namespace abstract {

struct Common
{
    virtual ~Common() { }
    struct Constrain {
        llvm::BasicBlock *code;
        std::map< llvm::Value *, llvm::Value * > map;
    };

    /*
     * Get the LLVM type that represents the abstract type corresponding to the
     * concrete type t. Must be idempotent, i.e. abstract( abstract( x ) ) ==
     * abstract( x ) for all x. The result must either be "t" itself, or it
     * must be an abstract type (see isAbstract).
     */
    virtual llvm::Type *abstract( llvm::Type *t ) = 0;

    /*
     * This particular abstraction deals with types "%lart.domain.*" --
     * i.e. if domain is "interval" then values of type
     * "%lart.interval.*" are the responsibility of this abstraction.
     */
    virtual Domain::Value domain() const = 0;

    /*
     * Construct a basic block that compute refined abstract values given the
     * condition that "v" is a subset of "constraint". Constraint must be an
     * abstract constant, v is the constrained value. Constraining abstract
     * value arises from branching instructions, where if a particular branch
     * has been taken, this may have implication for the values of the
     * variables that were involved in the decision to take the particular
     * branch. This applies in situations where a conditional branch may take
     * both routes -- i.e. the abstraction was not sufficiently precise to pick
     * just one.
     */
    virtual Constrain constrain( llvm::Value *v, llvm::Value *constraint ) = 0;

    /* The outside interface of abstraction passes.
     * Translate an abstract intrinsic call to a code block computing the effect of the
     * abstract equivalent. Where 'args' are already abstracted arguments for instruction.
     * Returns resulting value of abstracted instruction.
     */
    virtual llvm::Value *process( llvm::CallInst *, std::vector< llvm::Value * > & args ) = 0;

    /* Decides whether a type is corresponding to this abstraction. */
    virtual bool is( llvm::Type * ) = 0;

    virtual bool is( llvm::Value *v ) {
        return is( v->getType() );
    }

    virtual llvm::Type *abstract( llvm::Value *v ) {
        return abstract( v->getType() );
    }
};

}
}
