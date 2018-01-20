/* TAGS: min c++ */
/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>

#include <cassert>

int main() {
    _SYM int x;
    int y = 0;
    assert( x != y ); /* ERROR */
}
