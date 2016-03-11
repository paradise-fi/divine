#ifndef _PDCLIB_THREADCONFIG_H
#define _PDCLIB_THREADCONFIG_H
#include <_PDCLIB_aux.h>
#include <_PDCLIB_config.h>

#include <pthread.h>

_PDCLIB_BEGIN_EXTERN_C
#define _PDCLIB_THR_T pthread_t
#define _PDCLIB_CND_T pthread_cond_t
#define _PDCLIB_MTX_T pthread_mutex_t

#define _PDCLIB_TSS_DTOR_ITERATIONS 5
#define _PDCLIB_TSS_T pthread_key_t

typedef pthread_once_t _PDCLIB_once_flag;
#define _PDCLIB_ONCE_FLAG_INIT {_PTHREAD_ONCE_SIG_init, {0}}

_PDCLIB_END_EXTERN_C
#endif
