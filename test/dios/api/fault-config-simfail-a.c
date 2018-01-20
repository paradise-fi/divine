/* TAGS: min c */
#include <dios.h>
#include <assert.h>
#include <stdlib.h>

extern unsigned char const *_DiOS_fault_cfg;

int main() {
    int ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
    assert( ret >= 0 );
    int *a = malloc( sizeof( int ) );
    assert( a );
    free( a );
    ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_SimFail );
    assert( ret >= 0 );
    int *b = malloc( sizeof( int ) );
    assert( b ); /* ERROR */
    free( b );
}

