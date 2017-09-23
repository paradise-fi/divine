/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>

#include <assert.h>

int main() {
    _SYM short x;
    x %= 5;
    while ( true )
        x = (x + 1) % 5;
}
