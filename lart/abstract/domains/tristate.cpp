// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/tristate.h>
#include <lart/abstract/intrinsic.h>

namespace lart {
namespace abstract {

using Arguments = std::vector< llvm::Value * >;

llvm::Value * TristateDomain::process( llvm::CallInst * i, Arguments & args ) {
    assert( intrinsic::domain( i ).value() == Domain::Value::Tristate );
    auto name = "__abstract_tristate_" + intrinsic::name( i );

    llvm::Module * m = i->getParent()->getParent()->getParent();

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );
    assert( fn->arg_size() == args.size() );

    llvm::IRBuilder<> irb( i );
    return irb.CreateCall( fn, args );
}

} // namespace abstract
} // namespace lart
