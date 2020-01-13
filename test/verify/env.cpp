/* TAGS: min c++ */
/* VERIFY_OPTS: -EFOO=blabla -EBAR=0 */

#include <cstdlib>
#include <string>
#include <cassert>

using namespace std::literals;

int main() {
    assert( getenv( "FOO" ) == "blabla"s );
    assert( getenv( "BAR" ) == "0"s );
}
