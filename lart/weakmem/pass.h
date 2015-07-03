// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

#include <llvm/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>

namespace lart {
namespace weakmem {

/*
 * Replace non-scalar load and store instructions with series of scalar-based
 * ones, along with requisite aggregate value (de)composition.
 */
struct ScalarMemory : llvm::ModulePass
{
    static char ID;
    ScalarMemory() : llvm::ModulePass( ID ) {}
    bool runOnModule( llvm::Module &m ) {
        return false;
    }
};

struct Substitute : llvm::ModulePass
{
    static char ID;

    enum Type { TSO, PSO };
    Type _type;

    Substitute( Type t ) : llvm::ModulePass( ID ), _type( t ) {}
    virtual ~Substitute() {}

    void transform( llvm::Function *f ) {}
    bool runOnModule( llvm::Module &m ) {
        for ( auto f = m.begin(); f != m.end(); ++ f )
            transform( f );
        return true;
    }
};

}
}
