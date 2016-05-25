// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

#include <lart/support/meta.h>

#ifndef LART_DIVINE_PASSES_H
#define LART_DIVINE_PASSES_H

namespace lart {
    namespace divine {

        PassMeta interruptPass();
        PassMeta functionMetaPass();

        inline std::vector< PassMeta > passes() {
            return { interruptPass(), functionMetaPass() };
        }
    }
}

#endif
