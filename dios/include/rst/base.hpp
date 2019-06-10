// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    // Each abstract domain needs to inherit from Base
    // to maintain domain index
    struct Base
    {
        // Placeholder constructor for LART to initialize domain index
        _LART_INTERFACE Base( uint8_t id = 0 ) noexcept : id( id ) {};

        uint8_t id;
    };

} // namespace __dios::rst::abstract
