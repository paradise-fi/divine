#include <stdlib.h>

int main() {
    int x = 42;
    int y = ((short *)&x)[1];
    if ( y == 1 )
        exit( 1 );
    return 0;
}
