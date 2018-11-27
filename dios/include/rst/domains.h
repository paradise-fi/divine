#pragma once
#include <rst/lart.h>

#define _SYM __attribute__((__annotate__("lart.abstract.return.sym")))
#define _STAR __attribute__((__annotate__("lart.abstract.return.star")))
#define _ZERO __attribute__((__annotate__("lart.abstract.return.zero")))
#define _MSTRING __attribute__((__annotate__("lart.abstract.return.mstring")))

#ifdef __cplusplus
extern "C" {
#endif
    uint64_t __sym_val_i64();
    uint32_t __sym_val_i32();
    uint16_t __sym_val_i16();
    uint8_t __sym_val_i8();

    float __sym_val_float32();
    double __sym_val_float64();

    uint64_t __star_val_i64();
    uint32_t __star_val_i32();
    uint16_t __star_val_i16();
    uint8_t __star_val_i8();

    char * __mstring_val( const char * buff, unsigned buff_len );
#ifdef __cplusplus
}
#endif

