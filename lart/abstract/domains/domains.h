// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <brick-data>

namespace lart {
namespace abstract {

using brick::data::Bimap;

enum class Domain {
    LLVM,
    Tristate,
    Symbolic,
    Zero
};

const Bimap< Domain, std::string > DomainTable = {
     { Domain::LLVM, "llvm" }
    ,{ Domain::Tristate, "tristate" }
    ,{ Domain::Symbolic, "sym" }
    ,{ Domain::Zero, "zero" }
};

inline std::ostream& operator<<(std::ostream& os, const Domain& domain) {
    os << DomainTable[ domain ];
    return os;
}

} // namespace abstract
} // namespace lart
