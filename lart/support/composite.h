// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

#include <lart/support/pass.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <vector>

namespace lart {

struct CompositePass : lart::Pass
{
    std::vector< lart::Pass * > _passes;
    void append( lart::Pass *p ) { _passes.push_back( p ); }
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        auto result = llvm::PreservedAnalyses::all();
        for ( auto p : _passes )
            result.intersect( p->run( m ) );
        return result;
    }
};

}
