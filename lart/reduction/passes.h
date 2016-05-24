// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <lart/support/meta.h>

#ifndef LART_REDUCTION_PASSES_H
#define LART_REDUCTION_PASSES_H

namespace lart {
    namespace reduction {

        PassMeta paroptPass();
        PassMeta maskPass();
        PassMeta allocaPass();
        PassMeta registerPass();
        PassMeta globalsPass();
        PassMeta silentPass();

        inline std::vector< PassMeta > passes() {
            return { paroptPass(), maskPass(), allocaPass(), registerPass(), globalsPass(), silentPass() };
        }
    }
}

#endif
