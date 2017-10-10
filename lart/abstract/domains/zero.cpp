// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/util.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

namespace {
    static const std::string prefix = "__abstract_zero_";

    template< size_t idx >
    inline llvm::Type * argType( llvm::Instruction * i ) {
        return i->getOperand( idx )->getType();
    }

    std::string constructFunctionName( llvm::CallInst * call ) {
        auto intr = intrinsic::parse( call->getCalledFunction() ).value();
        std::string name = prefix + intr.name;
        if ( intr.name == "load" ) {
            //do nothing
        } else if ( ( intr.name == "lift" ) ||
                    ( intr.name == "lower" ) )
        {
            name += "_" + llvmname( argType< 0 >( call ) );
        } else if ( call->getCalledFunction()->arg_size() > 0 && argType< 0 >( call )->isPointerTy() ) {
            name += "_p";
        }
        return name;
    }
}

llvm::Value * Zero::process( llvm::CallInst * call, Values & args ) {
    auto name = constructFunctionName( call );
    auto m = getModule( call );

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );

    llvm::IRBuilder<> irb( call );
    return irb.CreateCall( fn, args );
}

bool Zero::is( llvm::Type * type ) {
    return zero_type == type;
}

llvm::Type * Zero::abstract( llvm::Type * type ) {
    return setPointerRank( zero_type, getPointerRank( type ) );
}

} // namespace abstract
} // namespace lart
