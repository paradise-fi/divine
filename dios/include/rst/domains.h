#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    uint64_t __sym_val_i64( void );
    uint32_t __sym_val_i32( void );
    uint16_t __sym_val_i16( void );
    uint8_t __sym_val_i8( void );
#ifdef __cplusplus
}
#endif
