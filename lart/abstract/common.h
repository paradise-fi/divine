// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <set>

#ifndef LART_ABSTRACT_COMMON
#define LART_ABSTRACT_COMMON

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
     * Translate an instruction to a code block computing the effect of the
     * abstract equivalent.
     */
    virtual void lower( llvm::Instruction *i ) = 0;

    /*
     * This particular abstraction deals with types "%lart.qualifier.*" --
     * i.e. if typeQualifier is "interval" then values of type
     * "%lart.interval.*" are the responsibility of this abstraction.
     */
    virtual std::string typeQualifier() = 0;

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

    /* The outside interface of abstraction passes. */
    virtual void process( llvm::Module &m, std::set< llvm::Value * > );
    virtual void process( llvm::Instruction *i );

    virtual llvm::Type *abstract( llvm::Value *v ) {
        return abstract( v->getType() );
    }
};

}
}

#endif
