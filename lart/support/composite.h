// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

#include <llvm/PassManager.h>
#include <llvm/IR/Module.h>
#include <vector>

namespace lart {

struct CompositePass : llvm::ModulePass
{
    static char ID;

    CompositePass() : llvm::ModulePass( ID ) {}

    std::vector< llvm::ModulePass * > _passes;
    void append( llvm::ModulePass *p ) { _passes.push_back( p ); }
    bool runOnModule( llvm::Module &m ) {
        bool result = false;
        for ( auto p : _passes )
            result = p->runOnModule( m ) || result;
        return result;
    }
};

}
