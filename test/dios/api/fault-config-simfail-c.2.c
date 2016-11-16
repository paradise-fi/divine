// VERIFY_OPTS: -o force-nofail:malloc
#include <dios.h>
#include <assert.h>
#include <stdlib.h>

extern unsigned char const *_DiOS_fault_cfg;

int main() {
    int ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_SimFail );
    assert( ret == _DiOS_FC_ELocked );
    int *a = malloc( sizeof( int ) );
    assert( a );
    free( a );
}

