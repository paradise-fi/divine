// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <llvm/PassManager.h>

#include <lart/aa/andersen.h>

namespace lart {
namespace aa {


struct Pass : llvm::ModulePass
{
    enum Type { Andersen };

    static char ID;
    Type _type;
    Pass( Type t ) : llvm::ModulePass( ID ), _type( t ) {}
    virtual ~Pass() {}

    bool runOnModule( llvm::Module &m ) {
        switch ( _type ) {
            case Andersen: {
                aa::Andersen a;
                a.build( m );
                a.solve();
                a.annotate( m );
                return true;
            }
        }
        return false;
    }
};

}
}
