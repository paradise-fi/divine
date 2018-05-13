#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    void __lart_stash( uintptr_t );
    uintptr_t __lart_unstash();
#ifdef __cplusplus
}
#endif
