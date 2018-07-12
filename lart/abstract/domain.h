// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

namespace lart::abstract {

namespace detail {
    using DomainData = std::tuple< std::string >;
}

struct Domain : detail::DomainData {
    using detail::DomainData::DomainData;

    std::string name() const noexcept { return std::get< std::string >( *this ); }

    static Domain Concrete() { return Domain( "concrete" ); }
    static Domain Tristate() { return Domain( "tristate" ); }
    static Domain Unknown() { return Domain( "unknown" ); }
};


} // namespace lart::abstract
