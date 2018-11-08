#include <lart/abstract/domain.h>

#include <lart/support/util.h>

#include <optional>
#include <iostream>

namespace lart {
namespace abstract {

using namespace llvm;

static const Bimap< DomainKind, std::string > KindTable = {
     { DomainKind::scalar , "scalar"  }
    ,{ DomainKind::pointer, "pointer" }
    ,{ DomainKind::string , "string"  }
    ,{ DomainKind::custom , "custom"  }
};

Domain DomainMetadata::domain() const {
    auto meta = glob->getMetadata( abstract_domain_tag );
    auto &tag = cast< MDNode >( meta->getOperand( 0 ) )->getOperand( 0 );
    return Domain( cast< MDString >( tag )->getString().str() );
}

DomainKind DomainMetadata::kind() const {
    auto meta = glob->getMetadata( abstract_domain_kind );
    auto data = cast< MDTuple >( meta->getOperand( 0 ) );
    return KindTable[ cast< MDString >( data->getOperand( 0 ) )->getString().str() ];
}

Type * DomainMetadata::base_type() const {
    return glob->getType()->getPointerElementType()->getStructElementType( base_type_offset );
}

llvm::Value * DomainMetadata::default_value() const {
    auto type = base_type();
    if ( type->isPointerTy() )
        return ConstantPointerNull::get( cast< PointerType >( type ) );
    if ( type->isIntegerTy() )
        return ConstantInt::get( type, 0 );
    UNREACHABLE( "Unsupported base type." );
}

bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom ) {
    auto dm = domain_metadata( *inst->getModule(), dom );

    switch ( dm.kind() ) {
        case DomainKind::scalar:
            return true; // TODO filter unsupported instructions
        case DomainKind::pointer:
            return true; // TODO filter unsupported instructions
        case DomainKind::string:
            return util::is_one_of< LoadInst, StoreInst, GetElementPtrInst, CallInst >( inst );
        case DomainKind::custom:
        default:
            UNREACHABLE( "Unsupported domain transformation." );
    }
}

DomainMetadata domain_metadata( Module &m, Domain dom ) {
    std::optional< DomainMetadata > meta;

    global_variable_walker( m, [&] ( auto glob, auto anno ) {
        if ( anno.name() == dom.name() )
            meta = DomainMetadata( glob );
    } );

    ASSERT( meta && "Domain specification was not found." );
    return meta.value();
}

} // namespace abstract
} // namespace lart


