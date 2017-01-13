/* VERIFY_OPTS: --symbolic */

#include <assert.h>

#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    __sym int x;
    x %= 5;
    while ( true )
        x = (x + 1) % 5;
}
