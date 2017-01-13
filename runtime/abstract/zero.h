#ifndef __ABSTRACT_ZERO_H__
#define __ABSTRACT_ZERO_H__

#include "common.h"
#include "tristate.h"

namespace abstract {

struct Zero {
    enum Domain { ZeroValue, NonzeroValue, Unknown };

    Zero( Domain val = Unknown ) : value( val ) { }

    Domain value;
};

#define __abstract_zero_lift( name, type ) \
Zero *  __abstract_zero_lift_##name( type i ) _ROOT _NOTHROW;

#define __abstract_zero_lower( name, type ) \
type __abstract_zero_lower_##name( Zero * val ) _ROOT _NOTHROW;

extern "C" {
    Zero ** __abstract_zero_alloca() _ROOT _NOTHROW;

    Zero * __abstract_zero_load( Zero ** a ) _ROOT _NOTHROW;

    void __abstract_zero_store( Zero * val, Zero ** ptr ) _ROOT _NOTHROW;

    __abstract_zero_lift( i1, bool )
    __abstract_zero_lift( i8, uint8_t )
    __abstract_zero_lift( i16, uint16_t )
    __abstract_zero_lift( i32, uint32_t )
    __abstract_zero_lift( i64, uint64_t )

    __abstract_zero_lower( i1,  bool )
    __abstract_zero_lower( i8,  uint8_t )
    __abstract_zero_lower( i16, uint16_t )
    __abstract_zero_lower( i32, uint32_t )
    __abstract_zero_lower( i64, uint64_t )

    Zero * __abstract_zero_add( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_sub( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_mul( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_sdiv( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_udiv( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_urem( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_srem( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_and( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_or( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_xor( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_shl( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_lshr( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_ashr( Zero * a, Zero * b ) _ROOT _NOTHROW;

    // cast operators
    Zero * __abstract_zero_trunc( Zero * a ) _ROOT _NOTHROW;
    Zero * __abstract_zero_zext( Zero * a ) _ROOT _NOTHROW;
    Zero * __abstract_zero_sext( Zero * a ) _ROOT _NOTHROW;
    Zero * __abstract_zero_bitcast( Zero * a ) _ROOT _NOTHROW;
    Zero ** __abstract_zero_bitcast_p( Zero ** a ) _ROOT _NOTHROW;

    // icmp operators
    Zero * __abstract_zero_icmp_eq( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_ne( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_ugt( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_uge( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_ult( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_ule( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_sgt( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_sge( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_slt( Zero * a, Zero * b ) _ROOT _NOTHROW;
    Zero * __abstract_zero_icmp_sle( Zero * a, Zero * b ) _ROOT _NOTHROW;

    Tristate * __abstract_zero_bool_to_tristate( Zero * a ) _ROOT _NOTHROW;

    Zero *__abstract_zero_assume( Zero * value, Zero * constraint, bool assume ) _ROOT _NOTHROW;
} // extern C
} // namespace abstract

#endif
