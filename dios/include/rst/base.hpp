// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    using BaseID = uint8_t;

    // Each abstract domain needs to inherit from Base
    // to maintain domain index
    struct tagged_abstract_domain_t
    {
        // Placeholder constructor for LART to initialize domain index
        _LART_NOINLINE tagged_abstract_domain_t( BaseID id = 0 ) noexcept
            : _id( id )
        {}

        BaseID _id;
    };

    struct abstract_domain_t
    {
        _LART_NOINLINE abstract_domain_t() noexcept {}
    };

    _LART_INLINE
    static BaseID domain( void * addr ) noexcept
    {
        return static_cast< tagged_abstract_domain_t * >( addr )->_id;
    }

    template< typename Domain >
    static bool is_domain( BaseID domain ) noexcept
    {
        return Domain()._id == domain;
    }

} // namespace __dios::rst::abstract
