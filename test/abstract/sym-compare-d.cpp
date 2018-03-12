/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int y = 0;

int main() {
    _SYM int x;
    // loaded value should be lifted
    assert( x != y ); /* ERROR */
}
