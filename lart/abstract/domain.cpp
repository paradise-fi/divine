#include <lart/abstract/domain.h>

#include <optional>
#include <iostream>

using namespace lart::abstract;
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
    ASSERT( type->isPointerTy() && "Non pointer base types are not yet supported." );
    return ConstantPointerNull::get( cast< PointerType >( type ) );
}

