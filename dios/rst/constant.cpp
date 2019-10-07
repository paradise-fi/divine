// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/constant.hpp>

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    template< typename C >
    _LART_INLINE C make_constant( C c ) noexcept
    {
        __lart_stash( static_cast< void * >( Constant::lift( c ) ) );
        return taint< C >( c );
    }

    extern "C" {
        _LART_SCALAR uint8_t __constant_lift_i8( uint8_t c ) { return make_constant< uint8_t >( c ); }
        _LART_SCALAR uint16_t __constant_lift_i16( uint16_t c ) { return make_constant< uint16_t >( c ); }
        _LART_SCALAR uint32_t __constant_lift_i32( uint32_t c ) { return make_constant< uint32_t >( c ); }
        _LART_SCALAR uint64_t __constant_lift_i64( uint64_t c ) { return make_constant< uint64_t >( c ); }
    }

} // namespace __dios::rst::abstract
