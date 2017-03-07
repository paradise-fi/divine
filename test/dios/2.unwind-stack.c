#include <assert.h>
#include <sys/divm.h>
#include <stddef.h>
#include <dios.h>

void foo() {
    __dios_unwind( NULL, NULL, NULL );
}

int main()
{
    foo();
    assert( 0 );
}
