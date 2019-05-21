/* TAGS: min c++ setjmp */
#include <setjmp.h>
#include <assert.h>

jmp_buf jb;
volatile bool jumped = false;

int x = 0;
struct X { ~X() { x = 1; } };

void bar() __attribute__((__noinline__)) {
    jumped = true;
    longjmp( jb, 42 );
}

void foo() __attribute__((__noinline__)) {
    X _;
    bar();
}

int main() {
    int r = setjmp( jb );
    if ( jumped )
        assert( r == 42 );
    else
        assert( r == 0 );

    if ( jumped ) {
        assert( x == 0 );
        return 0;
    }

    foo();
}


