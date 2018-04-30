/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>

int main() {
    int x = __sym_val_i64();
    x %= 5;
    while ( true )
        x = (x + 1) % 5;
}
