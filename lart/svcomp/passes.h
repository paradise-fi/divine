// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <lart/support/meta.h>

#ifndef LART_SVCOMP_PASSES_H
#define LART_SVCOMP_PASSES_H

namespace lart {
    namespace svcomp {

        PassMeta intrinsicPass();
        PassMeta volatilizePass();

        inline std::vector< PassMeta > passes() {
            return { volatilizePass(), intrinsicPass() };
        }
    }
}

#endif
