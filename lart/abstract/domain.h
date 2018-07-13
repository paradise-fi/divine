// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-data>

namespace lart::abstract {

namespace {
    using DomainData = std::tuple< std::string >;

    using brick::data::Bimap;
} // anonymous namespace

struct Domain : DomainData {
    using DomainData::DomainData;

    inline std::string name() const noexcept { return std::get< std::string >( *this ); }

    enum class Kind : uint8_t {
        scalar,
        pointer,
        string,
        custom
    };

    Kind kind( llvm::Module & ) const noexcept;

    static Domain Concrete() { return Domain( "concrete" ); }
    static Domain Tristate() { return Domain( "tristate" ); }
    static Domain Unknown() { return Domain( "unknown" ); }
};


} // namespace lart::abstract
