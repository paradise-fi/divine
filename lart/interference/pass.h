// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

#include <lart/support/id.h>

#include <llvm/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>

#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace lart {
namespace interference {

struct Pass : llvm::ModulePass
{
    static char ID;
    Pass() : llvm::ModulePass( ID ) {}
    virtual ~Pass() {}

    std::unordered_map< llvm::Instruction *, std::unordered_set< llvm::Value * > > _interference;

    void annotate( llvm::Function *f );
    void clear() { _interference.clear(); }
    void propagate( llvm::Instruction *def, llvm::Value *use );

    bool runOnModule( llvm::Module &m ) {
        updateIDs( m );
        int total = 0;
        for ( auto f = m.begin(); f != m.end(); ++ f )
            ++ total;

        int i = 0;
        for ( auto f = m.begin(); f != m.end(); ++ f ) {
            std::cerr << " [" << (++i) << "/" << total << "] annotating function " << f->getName().str() << std::endl;
            annotate( f );
            clear();
        }

        return true;
    }
};

}
}
