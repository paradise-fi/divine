/* TAGS: min c */
#include <setjmp.h>
#include <assert.h>

int main() {
    jmp_buf jb;
    int r = setjmp( jb );
    assert( r == 0 );
}
