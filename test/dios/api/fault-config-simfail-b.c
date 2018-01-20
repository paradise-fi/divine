/* TAGS: min c */
#include <dios.h>
#include <assert.h>
#include <stdlib.h>

int main() {
    int orig = __dios_get_fault_config( _DiOS_SF_Malloc );
    int ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_Ignore );
    assert( ret == _DiOS_FC_EInvalidCfg );
    assert( orig == __dios_get_fault_config( _DiOS_SF_Malloc ) );
    ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_Abort );
    assert( ret == _DiOS_FC_EInvalidCfg );
    assert( orig == __dios_get_fault_config( _DiOS_SF_Malloc ) );
    ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_Report );
    assert( ret == _DiOS_FC_EInvalidCfg );
    assert( orig == __dios_get_fault_config( _DiOS_SF_Malloc ) );
    ret = __dios_configure_fault( _DiOS_SF_Last, _DiOS_FC_SimFail );
    assert( ret == _DiOS_FC_EInvalidFault );
    assert( orig == __dios_get_fault_config( _DiOS_SF_Malloc ) );

    ret = __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_SimFail );
    int *b = malloc( sizeof( int ) );
    assert( b ); /* ERROR */
    free( b );
}
