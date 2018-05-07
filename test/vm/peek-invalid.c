/* TAGS: min c */
#include <sys/divm.h>
#include <assert.h>

int main()
{
    int *a;
    __vm_peek( a, _VM_ML_User ); /* ERROR */
    return 0;
}
