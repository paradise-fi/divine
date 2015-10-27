// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <lart/abstract/trivial.h>
#include <lart/abstract/interval.h>
#include <lart/support/pass.h>

#include <llvm/Pass.h>

namespace lart {
namespace abstract {

struct Pass : lart::Pass
{
    enum Type { Trivial, Interval };

    Type _type;
    Pass( Type t ) : _type( t ) {}

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) {
        std::set< llvm::Value * > vals;
        switch ( _type ) {
            case Trivial: {
                abstract::Trivial t;
                t.process( m, vals );
                break;
            }
            case Interval: {
                abstract::Interval t;
                t.process( m, vals );
                break;
            }
        }
        return llvm::PreservedAnalyses::none();
    }
};

}
}
