/* TAGS: min c */
#include <sys/divm.h>
#include <assert.h>
#include <stdlib.h>

int main()
{
    int *a = malloc( 4 );
    free( a );
    __vm_poke( a, _VM_ML_User, 0 ); /* ERROR */
    return 0;
}
