#include "divine-llvm.h"

int main() {
    void *str;
 restart:
    str = malloc( 10 );
    if ( str ) {
        trace( "got 10 bytes there" );
    } else {
        trace( "no luck, malloc fail!" );
        goto restart;
    }
    free( str );
    goto restart;
    return 0;
}
