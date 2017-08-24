// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/util.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

namespace {
    const std::string prefix = "__abstract_";
    std::string constructFunctionName( const llvm::Instruction & /*intr*/ ) {
        /*std::string name = prefix + intr.domain()->name() + "_" + intr.name();
        if ( intr.name() == "load" ) {
            //do nothing
        }
        else if ( isLift( intr ) || isLower( intr ) ) {
            auto tn = TypeName( llvm::cast< llvm::StructType >( intr.argType< 0 >() ) );
            name +=  "_" + tn.base().name();
        }
        else if ( intr.declaration()->arg_size() > 0 && intr.argType< 0 >()->isPointerTy() ) {
            name += "_p";
        }
        return name;*/
        NOT_IMPLEMENTED();
    }
}

llvm::Value * Zero::process( llvm::CallInst *, Values & ) {
    /*auto intr = Intrinsic( i );
    assert( (*intr.domain()) == (*domain()) );
    auto name = constructFunctionName( intr );
    llvm::Module * m = getModule( i );

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );
    assert( fn->arg_size() == args.size() );

    llvm::IRBuilder<> irb( i );
    return irb.CreateCall( fn, args );*/
    NOT_IMPLEMENTED();
}

bool Zero::is( llvm::Type * type ) {
    return zero_type == type;
}

llvm::Type * Zero::abstract( llvm::Type * type ) {
    return type->isPointerTy() ? zero_type->getPointerTo() : zero_type;
}

} // namespace abstract
} // namespace lart
