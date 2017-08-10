#include <stdio.h>

void foo( int x ) {
    void *y = &&x1;
    if ( x )
        y = &&x2;
  x1:
    goto *y;
  x2:
    return;
}

int main() {
    foo( 0 );
}
