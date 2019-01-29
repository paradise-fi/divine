/* TAGS: c min */
/* VERIFY_OPTS: */

#include <assert.h>

// V: undef
// V: ignored V_OPT: -o ignore:control

int main() {
    _Bool b;
    int v = b ? 1 : 2; /* ERR_undef */
    assert( v == 2 );
}
