// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once
DIVINE_RELAX_WARNINGS
#include <llvm/IR/BasicBlock.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace analysis {

struct BBEdge {
    using BB = llvm::BasicBlock;

    BBEdge( BB * from, BB * to ) : from( from ), to( to ) {}

    void hide() {
        from->dump();
        assert( from->getUniqueSuccessor() );
        from->getTerminator()->setSuccessor( 0, from );
    }

    void show() {
        assert( from->getUniqueSuccessor() );
        from->getTerminator()->setSuccessor( 0, to );
    }

    BB * from;
    BB * to;

};

} // namespace analysis
} // namespace lart
