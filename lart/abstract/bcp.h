// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>
// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>

/*
 * Backward Constraint Propagation
 *
 * This class expands calls to @lart.tristate.assume based on the domain(s) of
 * variables involved in derivation of the assume'd value.
 *
 * Calls to @lart.assume take an LLVM variable and an LLVM constant (both of
 * types %lart.tristate), and assert that the variable has the same value as
 * the constant. This knowledge is derived by the VA pass, and is based on
 * recent branching history (if a branch has been taken, the value must have
 * been what the assume states).
 *
 * The BCP pass "unrolls" the calls to @lart.assume based on how the decision
 * variable was obtained. Consider
 *
 * %cond = call %lart.tristate @lart.interval.icmp.slt(... %x, ... %y)
 * ...
 * call void @lart.assume(... %cond, %lart.tristate { 1 })
 *
 * BCP would then add calls like
 *
 * %cond1 = call @lart.tristate.assume(... %cond, %lart.tristate { 1 }, ... %cond)
 * %x1 = call @lart.interval.assume(... %cond, %lart.tristate { 1 }, ... %x)
 * %y1 = call @lart.interval.assume(... %cond, %lart.tristate { 1 }, ... %y)
 *
 * and patch up any subsequent uses of %cond with %cond1, and %x and %y with
 * %x1 and %y1 respectively, inserting new PHI nodes as necessary.
 *
 * The subsequent abstraction lowering passes can then easily generate code for
 * @lart.<domain>.assume that do not interact with data flow, they just compute
 * the constrained values based on looking at how %cond was derived (i.e. that
 * it is a result of @lart.interval.icmp.slt).
 */

namespace lart {
namespace abstract {

struct BCP : lart::Pass {

    virtual ~BCP() {}

	static PassMeta meta() {
    	return passMeta< BCP >( "BCP",
              "Backward Constraint Propagation expands calls to @lart.tristate.assume" );
    }

	llvm::PreservedAnalyses run( llvm::Module &m ) {
        process( m );
        return llvm::PreservedAnalyses::none();
    }

    void process( llvm::Module & m );
    void process( llvm::Instruction * inst );
};

PassMeta bcp_pass() {
    return BCP::meta();
}

} // namespace abstract
} // namespace lart

