/* TAGS: min c++ */
/* VERIFY_OPTS: --stdin $SRC_DIR/stdin.txt -o nofail:malloc */

#include <iostream>
#include <cassert>

int main() {
    int x, y, z;
    std::cin >> x >> y >> z;
    assert( std::cin.good() );
    assert( x == 42 );
    assert( y == 16 );
    assert( z == -4 );
}
