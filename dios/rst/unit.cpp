// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/unit.hpp>

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    template struct Unit< PointerBase >;

    template< typename C >
    _LART_INLINE C make_unit() noexcept
    {
        return make_abstract< C, Unit< PointerBase > >();
    }

    extern "C" {
        _LART_SCALAR uint8_t __unit_val_i8() { return make_unit< uint8_t >(); }
        _LART_SCALAR uint16_t __unit_val_i16() { return make_unit< uint16_t >(); }
        _LART_SCALAR uint32_t __unit_val_i32() { return make_unit< uint32_t >(); }
        _LART_SCALAR uint64_t __unit_val_i64() { return make_unit< uint64_t >(); }
    }

} // namespace __dios::rst::abstract
