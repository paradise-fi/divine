#include <lart/abstract/domain.h>

#include <brick-llvm>
#include <optional>
#include <iostream>

using namespace lart::abstract;
using namespace llvm;

static const Bimap< Domain::Kind, std::string > KindTable = {
     { Domain::Kind::scalar , "scalar"  }
    ,{ Domain::Kind::pointer, "pointer" }
    ,{ Domain::Kind::string , "string"  }
    ,{ Domain::Kind::custom , "custom"  }
};

template< typename Yield >
auto global_variable_walker( Module &m, Yield yield ) {
    brick::llvm::enumerateAnnosInNs< GlobalVariable >( "lart.abstract.domain.tag", m, yield );
}

StringRef get_kind_name( GlobalVariable *val ) {
    auto meta = val->getMetadata( "lart.abstract.domain.kind" );
    auto data = cast< MDTuple >( meta->getOperand( 0 ) );
    return cast< MDString >( data->getOperand( 0 ) )->getString();
}

Domain::Kind Domain::kind( Module &m ) const noexcept {
    const auto &tag = name();

    std::optional< Domain::Kind > result;

    global_variable_walker( m, [tag, &result] ( auto val, auto anno ) {
        if ( anno.name() == tag )
            result = KindTable[ get_kind_name( val ) ];
    });

    ASSERT( result.has_value() && "Domain specification was not found." );
    return result.value();
}


