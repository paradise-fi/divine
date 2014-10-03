// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <llvm/IR/Module.h>

namespace lart {
namespace aa {

struct PointsTo;

struct Pointer {
    PointsTo pointsTo();
};

struct Location {
    enum Type { Pointer, Scalar };
    /* TODO: field sensitivity */
    Type type();
    Pointer asPointer(); /* valid only if type() == Pointer */
    bool operator==( const Location &other );
};

struct PointsTo {
    struct iterator {
        Location operator*();
    };
    iterator begin();
    iterator end();
};

/*
 * This class provides convenient access to the metadata that encodes pointer
 * information in LLVM bitcode files. The class itself does not cache anything,
 * all queries reflect the current state stored in the associated Module.
 */

struct Info {
    Info( llvm::Module *m );
    PointsTo pointsTo( llvm::Value *v );

private:
    llvm::Module *_module;
};

}
}
