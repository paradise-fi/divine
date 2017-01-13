/* VERIFY_OPTS: -DFOO=blabla -DBAR=0 */

#include <cstdlib>
#include <string>
#include <cassert>

using namespace std::literals;

int main() {
    assert( getenv( "FOO" ) == "blabla"s );
    assert( getenv( "BAR" ) == "0"s );
}
