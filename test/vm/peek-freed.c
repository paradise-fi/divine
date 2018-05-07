/* TAGS: min c */
#include <sys/divm.h>
#include <assert.h>
#include <stdlib.h>

int main()
{
    int *a = malloc( 4 );
    free( a );
    __vm_peek( a, _VM_ML_User ); /* ERROR */
    return 0;
}
