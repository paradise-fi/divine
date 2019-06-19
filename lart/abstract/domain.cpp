#include <lart/abstract/domain.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>

#include <optional>
#include <iostream>

namespace lart::abstract {

    using brick::data::Bimap;

    static const Bimap< DomainKind, std::string > KindTable = {
         { DomainKind::scalar , "scalar"  }
        ,{ DomainKind::pointer, "pointer" }
        ,{ DomainKind::content, "content"  }
    };

    std::string to_string( DomainKind kind ) { return KindTable[ kind ]; }

} // namespace lart::abstract


