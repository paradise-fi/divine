/* TAGS: min c */
#include <sys/divm.h>
#include <assert.h>

int main()
{
    int *a;
    __vm_poke( a, _VM_ML_User, 0 ); /* ERROR */
    return 0;
}
