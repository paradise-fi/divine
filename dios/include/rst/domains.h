#pragma once

#include <stdint.h>

/* Functions available to the user to define an abstract value
   of a given type (from a given domain). */
#ifdef __cplusplus
extern "C" {
#endif
    uint64_t __sym_val_i64( void );
    uint32_t __sym_val_i32( void );
    uint16_t __sym_val_i16( void );
    uint8_t __sym_val_i8( void );

    float __sym_val_float32( void );
    double __sym_val_float64( void );

    uint64_t __sym_lift_i64( uint64_t );
    uint32_t __sym_lift_i32( uint32_t );
    uint16_t __sym_lift_i16( uint16_t );
    uint8_t __sym_lift_i8( uint8_t );

    float __sym_lift_float32( float );
    double __sym_lift_float64( double );

    uint64_t __unit_val_i64( void );
    uint32_t __unit_val_i32( void );
    uint16_t __unit_val_i16( void );
    uint8_t __unit_val_i8( void );

    float __unit_val_float32( void );
    double __unit_val_float64( void );

    uint64_t __unit_lift_i64( uint64_t );
    uint32_t __unit_lift_i32( uint32_t );
    uint16_t __unit_lift_i16( uint16_t );
    uint8_t __unit_lift_i8( uint8_t );

    float __unit_lift_float32( float );
    double __unit_lift_float64( double );

    char * __mstring_val( char * buff, unsigned buff_len );

    uint8_t __constant_lift_i8( uint8_t c );
    uint16_t __constant_lift_i16( uint16_t c );
    uint32_t __constant_lift_i32( uint32_t c );
    uint64_t __constant_lift_i64( uint64_t c );
#ifdef __cplusplus
}
#endif
