// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <lart/abstract/common.h>

namespace lart {
namespace abstract {

struct Trivial : Common {
    virtual void lower( llvm::Instruction *i ) {}
    virtual Constrain constrain( llvm::Value *v, llvm::Value *constraint ) {
        return Constrain();
    }
    virtual llvm::Type *abstract( llvm::Type *t ) { return t; }
};

}
}
