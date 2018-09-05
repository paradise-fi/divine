// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-data>
#include <brick-llvm>

namespace lart::abstract {

enum class DomainKind : uint8_t {
    scalar,
    pointer,
    string,
    custom
};

namespace {
    using DomainData = std::tuple< std::string >;

    using brick::data::Bimap;

} // anonymous namespace

static constexpr char abstract_tag[] = "lart.abstract";
static constexpr char abstract_domain_tag[]   = "lart.abstract.domain.tag";
static constexpr char abstract_domain_kind[]  = "lart.abstract.domain.kind";

struct Domain : DomainData {
    using DomainData::DomainData;

    inline std::string name() const noexcept { return std::get< std::string >( *this ); }

    static Domain Concrete() { return Domain( "concrete" ); }
    static Domain Tristate() { return Domain( "tristate" ); }
    static Domain Unknown() { return Domain( "unknown" ); }
};

struct DomainMetadata {

    DomainMetadata( llvm::GlobalVariable * glob )
        : glob( glob )
    {}

    Domain domain() const;
    DomainKind kind() const;
    llvm::Type * base_type() const;

private:
    static constexpr size_t base_type_offset = 0;

    llvm::GlobalVariable * glob;
};

template< typename Yield >
auto global_variable_walker( llvm::Module &m, Yield yield ) {
    brick::llvm::enumerateAnnosInNs< llvm::GlobalVariable >( abstract_domain_tag, m, yield );
}

} // namespace lart::abstract
