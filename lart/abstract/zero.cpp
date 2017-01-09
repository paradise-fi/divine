// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/zero.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/types.h>

namespace lart {
namespace abstract {

namespace {
    const std::string prefix = "__abstract_";
    std::string constructFunctionName( const std::string & domain, const llvm::CallInst * inst ) {
        std::string name = prefix + domain + "_" + intrinsic::name( inst );
        if ( intrinsic::name( inst ) == "load" ) {
            //do nothing
        }
        else if ( inst->getNumArgOperands() > 0 && intrinsic::ty1( inst ).back() == '*' ) {
            name += "_p";
        }
        return name;
    }

    bool hasSpecifiedName( llvm::Type * type, const std::string & name ) {
        type = type->isPointerTy() ? type->getPointerElementType() : type;
        auto structTy = llvm::dyn_cast< llvm::StructType >( type );
        return structTy && structTy->hasName() && structTy->getName() == name;
    }
}

Zero::Zero( llvm::Module & m ) : tristate( m ) {
    zero_type = m.getFunction( "__abstract_zero_lift" )->getReturnType();
    // TODO solve merging of type names
    //zero_type = m.getContext().pImpl->NamedStructTypes.lookup( LLVMTypeName() )
    //             ->getPointerTo();
}

void Zero::process( llvm::CallInst * i, std::map< llvm::Value *, llvm::Value * > &vmap ) {
    if ( intrinsic::isAssume( i ) ) {
        vmap[ i ] = i;
        return; //skip
    }

    auto domain = intrinsic::domain( i ) == "tristate" ? tristate.domain() : this->domain();
    auto name = constructFunctionName( domain, i );
    llvm::Module * m = i->getParent()->getParent()->getParent();

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );

    std::vector < llvm::Value * > args;
    for ( auto &arg : i->arg_operands() ) {
        if ( types::isAbstract( arg->getType() ) && !vmap.count( arg ) )
            break;
        auto lowered = types::isAbstract( arg->getType() ) ? vmap[ arg ] : arg;

        if ( from_tristate_map.count( lowered ) )
            lowered = from_tristate_map[ lowered ];

        //change domain of an argument from tristate to zero
        if ( intrinsic::domain( i ) != "tristate" && tristate.is( lowered->getType() ) ) {
            auto ft = m->getFunction( "__abstract_zero_from_tristate" );
            llvm::IRBuilder<> irb( i );
            auto zero = irb.CreateCall( ft, lowered );
            from_tristate_map[ lowered ] = zero;
            args.push_back( zero );
        } else {
            args.push_back( lowered );
        }
    }

    //skip if do not have enough substituted arguments
    if ( i->getNumArgOperands() == args.size() ) {
        assert( fn->arg_size() == args.size() );
        llvm::IRBuilder<> irb( i );
        auto ncall = irb.CreateCall( fn, args );
        vmap[ i ] = ncall;
    }
}

bool Zero::is( llvm::Type * type ) {
    return hasSpecifiedName( type, LLVMTypeName() );
}

llvm::Type * Zero::abstract( llvm::Type * type ) {
    return type->isPointerTy() ? zero_type->getPointerTo() : zero_type;
}

Zero::Tristate::Tristate( llvm::Module & m ) {
    tristate_type = m.getFunction( "__abstract_tristate_create" )->getReturnType();
    // TODO solve merging of type names
    //tristate_type = m.getContext().pImpl->NamedStructTypes.lookup( LLVMTypeName() )
    //                 ->getPointerTo();
}

bool Zero::Tristate::is( llvm::Type * type ) {
    return hasSpecifiedName( type, LLVMTypeName() );
}

llvm::Type * Zero::Tristate::abstract( llvm::Type * type ) {
    assert( type->isIntegerTy( 1 ) );
    return type->isPointerTy() ? tristate_type->getPointerTo() : tristate_type;
}

} // namespace abstract
} // namespace lart
