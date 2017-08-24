// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/tristate.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

llvm::Value * TristateDomain::process( llvm::CallInst * call, Values & args ) {
    auto intr = intrinsic::parse( call->getCalledFunction() ).value();
    assert( intr.domain == domain() );
    auto name = "__abstract_tristate_" + intr.name;

    auto m = getModule( call );

    auto fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );

    llvm::IRBuilder<> irb( call );
    return irb.CreateCall( fn, args );
}

} // namespace abstract
} // namespace lart
