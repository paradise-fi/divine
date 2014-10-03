// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <lart/abstract/common.h>

namespace lart {
namespace abstract {

void Common::process( llvm::Module &m, std::set< llvm::Value * > ) {
    // _module = &m;

    // for ( auto v = m.global_begin(); v != m.global_end(); ++ v )
    // annotate( v, seen );

    for ( auto &f : m )
        for ( auto &b: f )
            for ( auto &i : b )
                process( &i );
}

void Common::process( llvm::Instruction *i ) {
}

}
}
