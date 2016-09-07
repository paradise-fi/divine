// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>

#pragma once

#include <lart/support/meta.h>

namespace lart {
    namespace abstract {

        PassMeta abstractionPass();

        inline std::vector< PassMeta > passes() {
            return { abstractionPass() };
        }
    }
}
