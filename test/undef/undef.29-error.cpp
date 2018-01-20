/* TAGS: min c++ */
#include <cstdlib>

struct Test {
    int x;
    int y;
    Test() : y( 0 ) { }
};

int main() {
    Test x;
    if ( x.y != 0 ) std::exit( 1 ); // OK
    if ( x.x != 0 ) std::exit( 1 ); /* ERROR */
    return 0;
}
