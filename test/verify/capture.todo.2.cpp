/* VERIFY_OPTS: --capture `dirname $1`/small:nofollow:/ */

#include <fstream>
#include <cassert>

int main() {
    std::ifstream is( "/a" );
    int x;
    is >> x;
    assert( is.good() );
    assert( x == 42 );
}

