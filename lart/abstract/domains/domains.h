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
        Struct,
        Tristate,
        Trivial,
        Symbolic,
        Zero,
        LastDomain = Zero
    };

    namespace {
    const brick::data::Bimap< Value, std::string > Values = {
         { Value::LLVM, "llvm" }
        ,{ Value::Struct, "struct" } // TODO rename/remove ?
        ,{ Value::Tristate, "tristate" }
        ,{ Value::Trivial, "triv" }
        ,{ Value::Symbolic, "sym" }
        ,{ Value::Zero, "zero" }
    };
    } // anonymous namespace

    using MaybeDomain = brick::types::Maybe< Domain::Value >;

    static std::string name( const Value & d ) {
        return Values[ d ];
    }

    static MaybeDomain value( const std::string & n ) {
        if ( Values.right().count( n ) )
            return MaybeDomain::Just( Values[ n ] );
        else
            return MaybeDomain::Nothing();
    }

} // namespce Domain

const Domain::Value LLVMDomain = Domain::Value::LLVM;

using DomainMap = std::map< size_t, Domain::Value >;
} // namespace abstract
} // namespace lart
