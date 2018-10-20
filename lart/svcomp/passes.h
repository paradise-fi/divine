// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <lart/support/meta.h>
DIVINE_UNRELAX_WARNINGS

#ifndef LART_SVCOMP_PASSES_H
#define LART_SVCOMP_PASSES_H

namespace lart {
    namespace svcomp {

        PassMeta svcompPass();
        PassMeta svcFixGlobalsPass();

        inline std::vector< PassMeta > passes() {
            return { svcompPass(), svcFixGlobalsPass() };
        }
    }
}

#endif
