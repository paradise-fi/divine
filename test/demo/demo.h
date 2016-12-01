#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#ifdef __divine__
#include <dios.h>

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#endif
