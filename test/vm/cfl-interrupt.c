/* TAGS: min c */
#include <stdbool.h>

void foo( bool x ) {
    if ( x ) {
        while ( true ) {
            foo( false );
        }
    }
}

int main() {
    foo( true );
}
