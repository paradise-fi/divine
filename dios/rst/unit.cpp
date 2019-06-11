// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/unit.hpp>

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    template struct Unit< true /* pointer_based */ >;

} // namespace __dios::rst::abstract

namespace {
    using Unit = __dios::rst::abstract::Unit< true >;

    template< typename T >
    _LART_INLINE T make_unit() noexcept {
        __lart_stash( Unit::lift_any() );
        return __dios::rst::abstract::taint< T >();
    }
} // anonymous namespace

extern "C" {
    _LART_SCALAR uint8_t __unit_val_i8() { return make_unit< uint8_t >(); }
    _LART_SCALAR uint16_t __unit_val_i16() { return make_unit< uint16_t >(); }
    _LART_SCALAR uint32_t __unit_val_i32() { return make_unit< uint32_t >(); }
    _LART_SCALAR uint64_t __unit_val_i64() { return make_unit< uint64_t >(); }
}

