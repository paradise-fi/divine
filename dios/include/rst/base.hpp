// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    using base_id_t = uint8_t;

    // Each abstract domain needs to inherit from abstract_domain base
    // either from tagged, which maintains domain index or from
    // abstract_domain_t that lets the responsibility for id on the domain.
    struct tagged_abstract_domain_t
    {
        // Placeholder constructor for LART to initialize domain index
        _LART_NOINLINE tagged_abstract_domain_t( base_id_t id = 0 ) noexcept
            : _id( id )
        {}

        base_id_t _id;
    };

    struct abstract_domain_t
    {
        _LART_NOINLINE abstract_domain_t() noexcept {}
    };

    _LART_INLINE
    static base_id_t domain( void * addr ) noexcept
    {
        return static_cast< tagged_abstract_domain_t * >( addr )->_id;
    }

    template< typename Domain >
    static bool is_domain( base_id_t domain ) noexcept
    {
        return Domain()._id == domain;
    }

    using abstract_value_t = tagged_abstract_domain_t *;

} // namespace __dios::rst::abstract
