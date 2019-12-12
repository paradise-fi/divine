// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/constant.hpp>

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    using abstract_t = void *;

    template< typename concrete_t >
    _LART_INLINE concrete_t make_constant( concrete_t c ) noexcept
    {
        return stash_abstract_value< concrete_t >( constant_t::lift( c ) );
    }

    extern "C" {
        _LART_SCALAR bool __constant_lift_i1( bool c ) { return make_constant< bool > ( c ); }
        _LART_SCALAR uint8_t __constant_lift_i8( uint8_t c ) { return make_constant< uint8_t >( c ); }
        _LART_SCALAR uint16_t __constant_lift_i16( uint16_t c ) { return make_constant< uint16_t >( c ); }
        _LART_SCALAR uint32_t __constant_lift_i32( uint32_t c ) { return make_constant< uint32_t >( c ); }
        _LART_SCALAR uint64_t __constant_lift_i64( uint64_t c ) { return make_constant< uint64_t >( c ); }

        abstract_t __lart_lift_constant_i1( bool v ) { return constant_t::lift( v ); }
        abstract_t __lart_lift_constant_i8(  uint8_t v ) { return constant_t::lift( v ); }
        abstract_t __lart_lift_constant_i16( uint16_t v ) { return constant_t::lift( v ); }
        abstract_t __lart_lift_constant_i32( uint32_t v ) { return constant_t::lift( v ); }
        abstract_t __lart_lift_constant_i64( uint64_t v ) { return constant_t::lift( v ); }
        abstract_t __lart_lift_constant_ptr( void * v ) { return constant_t::lift( v ); }
    }

} // namespace __dios::rst::abstract
