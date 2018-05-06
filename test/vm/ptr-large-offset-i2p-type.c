/* TAGS: min c */
#include <assert.h>
#include <stdint.h>
#include <sys/divm.h>

char array[20];

// TODO: our current clang does not generate line information for an indirect goto
int main()
{   /* ERROR */
    array[0] = 'a';
    array[10] = 'b';

    assert( *(array + 10) == 'b' );
    uint64_t aint = (uint64_t)array;
    aint += (uint64_t)(_VM_PT_Code - _VM_PT_Global) << _VM_PB_Off;
    void *achar = (void *)aint;
    void *labref = &&lab;
  lab:
    goto *achar;
}
