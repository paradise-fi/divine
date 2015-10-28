// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <lart/support/meta.h>

#ifndef LART_DIVINE_PASSES_H
#define LART_DIVINE_PASSES_H

namespace lart {
    namespace divine {

        PassMeta declsPass();

        inline std::vector< PassMeta > passes() {
            return { declsPass() };
        }
    }
}

#endif
