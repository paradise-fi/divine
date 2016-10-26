#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#ifdef __divine__
#include <dios.h>
#define print(X) __dios_trace_t(X)
#define printf(...) __dios_trace_f(__VA_ARGS__)

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#else
#define print(X) printf(X "\n")
#endif
