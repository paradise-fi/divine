/* TAGS: min c setjmp */
#include <setjmp.h>
#include <assert.h>
#include <stdbool.h>

jmp_buf jb;
volatile bool jumped = false;

void bar() __attribute__((__noinline__)) {
    jumped = true;
    longjmp( jb, 42 );
}

void foo() __attribute__((__noinline__)) {
    bar();
}

void baz( int a, int b ) {
    int r = setjmp( jb );
    if ( jumped )
        assert( r == 42 );
    else
        assert( r == 0 );

    if ( jumped )
        return;

    foo();
}

int main() {
    baz( 42, 16 );
}

