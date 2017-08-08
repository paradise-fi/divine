// -*- C++ -*- (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

#include <lart/support/meta.h>
#include <lart/support/util.h>

#ifndef LART_REDUCTION_PASSES_H
#define LART_REDUCTION_PASSES_H

namespace lart {
    namespace reduction {

        static constexpr const char *const silentTag = "lart.escape.silent";
        inline bool isSilent( llvm::Instruction &instr, unsigned silentID = 0 )
        {
            if ( !silentID )
                silentID = instr.getParent()->getParent()->getParent()->getMDKindID( silentTag );
            return instr.getMetadata( silentID );
        }

        PassMeta paroptPass();
        PassMeta maskPass();
        PassMeta allocaPass();
        PassMeta registerPass();
        PassMeta globalsPass();
        PassMeta staticTauMemPass();

        inline std::vector< PassMeta > passes() {
            return { paroptPass(), maskPass(), allocaPass(), registerPass(),
                     globalsPass(), staticTauMemPass() };
        }
    }
}

#endif
