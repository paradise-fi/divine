/* TAGS: min c */
/* EXPECT: --result error --symbol test_fun */
// NOTE: for some reason LLVM has wrong debug info for IndirectBr (wrong line
// number), so we can't use /* ERROR */
#include <stdio.h>

void test_fun( void *x ) {
    void *y = &&x2;
  x2:
    printf( "x1" );
    goto *x;
    printf( "x2" );
}

int main() {
    void *x = &&a2;
    test_fun( x );

  a1:
    printf( "a1" );
  a2:
    printf( "a2" );
    return 0;
}
