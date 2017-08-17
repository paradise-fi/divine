#define _ROOT __attribute__((__annotate__("divine.link.always")))

typedef unsigned char bool;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"

typedef struct Tristate { } Tristate;

Tristate * __abstract_tristate_create() _ROOT {}

Tristate * __abstract_tristate_lift( bool b ) _ROOT {}

bool __abstract_tristate_lower( Tristate * b ) _ROOT {}

typedef struct Abstract { } Abstract;

#define __abstract_test_lower_type( name, type ) \
type __abstract_test_lower_##name( Abstract * b ) _ROOT {}

#define __abstract_test_lift_type( name, type ) \
Abstract * __abstract_test_lift_##name( type t ) _ROOT {}

Abstract ** __abstract_test_alloca() _ROOT {}

Abstract * __abstract_test_load( Abstract ** b ) _ROOT {}

void __abstract_test_store( Abstract * a, Abstract ** b ) _ROOT {}

__abstract_test_lift_type( i1,  bool )
__abstract_test_lift_type( i8,  uint8_t )
__abstract_test_lift_type( i16,  uint16_t )
__abstract_test_lift_type( i32,  uint32_t )
__abstract_test_lift_type( i64,  uint64_t )

__abstract_test_lower_type( i1,  bool )
__abstract_test_lower_type( i8,  uint8_t )
__abstract_test_lower_type( i16, uint16_t )
__abstract_test_lower_type( i32, uint32_t )
__abstract_test_lower_type( i64, uint64_t )

Abstract * __abstract_test_add( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_sub( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_mul( Abstract * a,  Abstract * b) _ROOT {}

Abstract * __abstract_test_sdiv( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_udiv( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_urem( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_srem( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_and( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_or( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_xor( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_shl( Abstract * a, Abstract * b) _ROOT {}

Abstract * __abstract_test_lshr( Abstract * a, Abstract * b) _ROOT {}

// cast operators
Abstract * __abstract_test_trunc( Abstract * b) _ROOT {}

Abstract * __abstract_test_zext( Abstract * b) _ROOT {}

Abstract * __abstract_test_sext( Abstract * b) _ROOT {}

Abstract * __abstract_test_bitcast( Abstract * b) _ROOT {}

// icmp operators
Tristate * __abstract_test_icmp_eq( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_ne( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_ugt( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_uge( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_ult( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_ule( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_sgt( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_sge( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_slt( Abstract * a, Abstract * b) _ROOT {}

Tristate * __abstract_test_icmp_sle( Abstract * a, Abstract * b) _ROOT {}

#pragma GCC diagnostic pop
