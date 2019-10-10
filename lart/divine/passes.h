// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

#include <lart/support/meta.h>

#ifndef LART_DIVINE_PASSES_H
#define LART_DIVINE_PASSES_H

namespace lart {
    namespace divine {

        PassMeta memInterruptPass();
        PassMeta cflInterruptPass();
        PassMeta functionMetaPass();
        PassMeta autotracePass();
        PassMeta makeNativePass();
        PassMeta vaArgPass();
        PassMeta lowering();
        PassMeta lsda();
        PassMeta stubsPass();
        PassMeta abstractStubsPass();
        PassMeta leakCheck();
        PassMeta fuseCtorsPass();

        inline std::vector< PassMeta > passes() {
            return { fuseCtorsPass(), cflInterruptPass(), memInterruptPass(),
              functionMetaPass(), autotracePass(),  makeNativePass(), vaArgPass(),
              lowering(), stubsPass(), abstractStubsPass(), lsda(), leakCheck() };
        }
    }
}

#endif
