/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int foo() { return 0; }

int main() {
    _SYM int x;
    // returned value should be lifted
    assert( x != foo() ); /* ERROR */
}
