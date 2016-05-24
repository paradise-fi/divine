// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/aa/andersen.h>

namespace lart {
namespace aa {

struct Pass : lart::Pass
{
    enum Type { Andersen };

    Type _type;
    Pass( Type t ) : _type( t ) {}

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) {
        switch ( _type ) {
            case Andersen: {
                aa::Andersen a;
                a.build( m );
                a.solve();
                a.annotate( m );
            }
        }
        return llvm::PreservedAnalyses::all();
    }
};

}
}
