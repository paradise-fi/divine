// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

#include <lart/support/meta.h>

#ifndef LART_DIVINE_PASSES_H
#define LART_DIVINE_PASSES_H

namespace lart {
    namespace divine {

        PassMeta interruptPass();
        PassMeta functionMetaPass();
        PassMeta autotracePass();
        PassMeta makeNativePass();

        inline std::vector< PassMeta > passes() {
            return { interruptPass(), functionMetaPass(), autotracePass(),
                     makeNativePass() };
        }
    }
}

#endif
