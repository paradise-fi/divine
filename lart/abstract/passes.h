// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/support/meta.h>
#include <lart/abstract/abstraction.h>
#include <lart/abstract/assume.h>

namespace lart {
    namespace abstract {

        PassMeta full_abstraction_pass();
        PassMeta assume_pass();

        inline std::vector< PassMeta > passes() {
            return { full_abstraction_pass() };
        }
    }
}
