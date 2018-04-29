#pragma once
#include <rst/lart.h>

#define _SYM __attribute__((__annotate__("lart.abstract.sym")))
#define _ZERO __attribute__((__annotate__("lart.abstract.zero")))

extern "C"  {
    uint64_t __sym_val_i64();
    uint32_t __sym_val_i32();
    uint16_t __sym_val_i16();
    uint8_t __sym_val_i8();
}

