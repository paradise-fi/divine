// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {

inline bool tagModuleWithMetadata( llvm::Module &m, std::string tag ) {
    if ( m.getNamedMetadata( tag ) )
        return false;
    m.getOrInsertNamedMetadata( tag );
    return true;
}

inline bool tagFunctionWithMetadata( llvm::Function &fn, std::string tag ) {
    if ( fn.getMetadata( tag ) )
        return false;
    fn.setMetadata( tag, nullptr );
    return true;
}

}
