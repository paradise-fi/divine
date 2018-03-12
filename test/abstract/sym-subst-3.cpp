/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>

int main() {
    _SYM long x;
    x %= 5;
    while ( true )
        x = (x + 1) % 5;
}
