#pragma once
#include <rst/lart.h>

#define _SYM __attribute__((__annotate__("lart.abstract.return.sym")))
#define _STAR __attribute__((__annotate__("lart.abstract.return.star")))
#define _ZERO __attribute__((__annotate__("lart.abstract.return.zero")))
#define _MSTRING __attribute__((__annotate__("lart.abstract.return.mstring")))

#ifdef __cplusplus
extern "C" {
#endif
    uint64_t __sym_val_i64( void );
    uint32_t __sym_val_i32( void );
    uint16_t __sym_val_i16( void );
    uint8_t __sym_val_i8( void );

    float __sym_val_float32( void );
    double __sym_val_float64( void );

    uint64_t __star_val_i64( void );
    uint32_t __star_val_i32( void );
    uint16_t __star_val_i16( void );
    uint8_t __star_val_i8( void );

    char * __mstring_val( char * buff, unsigned buff_len );
#ifdef __cplusplus
}
#endif

