// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <brick-data>

namespace lart {
namespace abstract {

namespace Domain {
    using Type = uint16_t;
    enum class Value : Type {
        LLVM,
        Abstract,
        Tristate,
        Trivial,
        Symbolic,
        Zero,
        LastDomain = Zero
    };

    namespace {
    const brick::data::Bimap< Value, std::string > DomainMap = {
         { Value::LLVM, "llvm" }
        ,{ Value::Abstract, "abstract" }
        ,{ Value::Tristate, "tristate" }
        ,{ Value::Trivial, "trivial" }
        ,{ Value::Symbolic, "symbolic" }
        ,{ Value::Zero, "zero" }
    };
    } // empty namespace


    static std::string name( const Value & d ) {
        return DomainMap[ d ];
    }

    static Value value( const std::string & n ) {
        return DomainMap[ n ];
    }
}

} // namespace abstract
} // namespace lart
