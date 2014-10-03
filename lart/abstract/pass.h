// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <lart/abstract/trivial.h>
#include <lart/abstract/interval.h>

#include <llvm/PassManager.h>

namespace lart {
namespace abstract {

struct Pass : llvm::ModulePass
{
    enum Type { Trivial, Interval };

    static char ID;
    Type _type;
    Pass( Type t ) : llvm::ModulePass( ID ), _type( t ) {}
    virtual ~Pass() {}

    bool runOnModule( llvm::Module &m ) {
        std::set< llvm::Value * > vals;
        switch ( _type ) {
            case Trivial: {
                abstract::Trivial t;
                t.process( m, vals );
                return true;
            }
            case Interval: {
                abstract::Interval t;
                t.process( m, vals );
                return true;
            }
        }
        return false;
    }
};

}
}
