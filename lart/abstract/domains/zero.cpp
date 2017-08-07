// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

namespace {
    const std::string prefix = "__abstract_";
    std::string constructFunctionName( const Domain::Value domain, const llvm::CallInst * inst ) {
        std::string name = prefix + Domain::name( domain ) + "_" + intrinsic::name( inst );
        if ( intrinsic::name( inst ) == "load" ) {
            //do nothing
        }
        else if ( intrinsic::isLift( inst ) || intrinsic::isLower( inst ) ) {
            if ( domain != Domain::Value::Tristate )
                name +=  "_" + intrinsic::ty1( inst );
        }
        else if ( inst->getNumArgOperands() > 0 && intrinsic::ty1( inst ).back() == '*' ) {
            name += "_p";
        }
        return name;
    }
}

Zero::Zero( llvm::Module & m ) {
    zero_type = m.getFunction( "__abstract_zero_load" )->getReturnType();
}

llvm::Value * Zero::process( llvm::CallInst * i, std::vector< llvm::Value * > &args ) {
    auto domain = intrinsic::domain( i ).value() == Domain::Value::Tristate
                ? Domain::Value::Tristate
                : this->domain();
    // TODO rework use intrinsic::domain directly
    //assert( domain == this->domain() || domain == Domain::Value::Tristate );
    auto name = constructFunctionName( domain, i );
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
