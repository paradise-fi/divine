// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

namespace {
    const std::string prefix = "__abstract_";
    std::string constructFunctionName( const Intrinsic & intr ) {
        std::string name = prefix + Domain::name( intr.domain() ) + "_" + intr.name();
        if ( intr.name() == "load" ) {
            //do nothing
        }
        else if ( isLift( intr ) || isLower( intr ) ) {
            name +=  "_" + TypeBase::name( TypeBase::get( intr.argType< 0 > () ) );
        }
        else if ( intr.declaration()->arg_size() > 0 && intr.argType< 0 >()->isPointerTy() ) {
            name += "_p";
        }
        return name;
    }
}

Zero::Zero( llvm::Module & m ) {
    zero_type = m.getFunction( "__abstract_zero_load" )->getReturnType();
}

llvm::Value * Zero::process( llvm::CallInst * i, std::vector< llvm::Value * > &args ) {
    auto intr = Intrinsic( i );
    assert( intr.domain() == domain() );
    auto name = constructFunctionName( intr );
    llvm::Module * m = i->getParent()->getParent()->getParent();

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );
    assert( fn->arg_size() == args.size() );

    llvm::IRBuilder<> irb( i );
    return irb.CreateCall( fn, args );
}

bool Zero::is( llvm::Type * type ) {
    return zero_type == type;
}

llvm::Type * Zero::abstract( llvm::Type * type ) {
    return type->isPointerTy() ? zero_type->getPointerTo() : zero_type;
}

} // namespace abstract
} // namespace lart
