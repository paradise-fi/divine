/* TAGS: min c++ */
/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <assert.h>

int main() {
    _SYM int x;
    _SYM int y;
    assert( x > y ); /* ERROR */
}
