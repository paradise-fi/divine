/* TAGS: min c setjmp */
#include <setjmp.h>
#include <assert.h>
#include <stdbool.h>

volatile bool jumped = false;

int main() {
    jmp_buf jb;
    int r = setjmp( jb );
    if ( jumped )
        assert( r == 42 );
    else
        assert( r == 0 );

    if ( jumped )
        return 0;

    jumped = true;
    longjmp( jb, 42 );
}

