// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <lart/aa/andersen.h>

namespace lart {
namespace aa {

struct Pass
{
    enum Type { Andersen };

    Type _type;
    Pass( Type t ) : _type( t ) {}

    void run( llvm::Module &m ) {
        switch ( _type ) {
            case Andersen: {
                aa::Andersen a;
                a.build( m );
                a.solve();
                a.annotate( m );
            }
        }
    }
};

}
}
