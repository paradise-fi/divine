// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <brick-data>
#include <brick-types>

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
        ,{ Value::Trivial, "triv" }
        ,{ Value::Symbolic, "sym" }
        ,{ Value::Zero, "zero" }
    };
    } // empty namespace

    using MaybeDomain = brick::types::Maybe< Domain::Value >;

    static std::string name( const Value & d ) {
        return DomainMap[ d ];
    }

    static MaybeDomain value( const std::string & n ) {
        if ( DomainMap.right().count( n ) )
            return MaybeDomain::Just( DomainMap[ n ] );
        else
            return MaybeDomain::Nothing();
    }

} // namespce Domain
} // namespace abstract
} // namespace lart
