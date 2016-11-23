// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <set>

#ifndef LART_ABSTRACT_VA
#define LART_ABSTRACT_VA


namespace lart {
namespace abstract {

struct Domain {
    llvm::Type *type;

    std::string name() { // get the name of this domain
        auto n = type->getStructName();
        assert ( !n.empty() );
        assert( std::string( n.str(), 0, 6 ) == "%lart." );

        // extract the domain portion from %lart.<domain>[.subtype]
        std::string tn = std::string( n.str(), 7, n.str().length() );
        return std::string( tn, 0, tn.find( '.' ) == std::string::npos ?
                            tn.length() : tn.find( '.' ) );
    }
};

struct VA {
    void process( llvm::Module &m, std::map< llvm::Value *, Domain > );
    void process( llvm::Instruction *i );

};

}
}

#endif
