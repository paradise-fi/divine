/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

int x;

int main() {
    _SYM int in;
    x = in;
    int y = 0;
    assert( x != y ); /* ERROR */
}
