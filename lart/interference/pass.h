// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>

#include <lart/support/id.h>
#include <lart/support/pass.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace lart {
namespace interference {

struct Pass
{
    std::unordered_map< llvm::Instruction *, std::unordered_set< llvm::Instruction * > > _interference;

    void annotate( llvm::Function &f );
    void clear() { _interference.clear(); }
    void propagate( llvm::Instruction *def, llvm::Instruction *use );

    void run( llvm::Module &m ) {
        updateIDs( m );
        int total = 0;
        for ( auto f = m.begin(); f != m.end(); ++ f )
            ++ total;

        int i = 0;
        for ( auto &f : m ) {
            std::cerr << " [" << (++i) << "/" << total << "] annotating function " << f.getName().str() << std::endl;
            annotate( f );
            clear();
        }
    }
};

}
}
