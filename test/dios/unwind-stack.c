/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>
#include <stddef.h>
#include <dios.h>

void foo() {
    __dios_unwind( NULL, NULL, NULL );
    __dios_suicide(); /* there's nowhere to return */
}

int main()
{
    foo();
    assert( 0 );
}
