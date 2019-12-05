#pragma once
#include <stdint.h>

#define _LART_IGNORE_RET __attribute__((__annotate__("lart.transform.ignore.ret")))
#define _LART_IGNORE_ARG __attribute__((__annotate__("lart.transform.ignore.arg")))
#define _LART_FORBIDDEN __attribute__((__annotate__("lart.transform.forbidden")))

#ifdef __cplusplus
extern "C" {
#endif
    void __lart_stash( void * );
    void * __lart_unstash();
#ifdef __cplusplus
}
#endif