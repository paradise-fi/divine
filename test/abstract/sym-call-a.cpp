/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

bool is_zero( int v ) { return v == 0; }

int main() {
    int input = __sym_val_i32();
    assert( is_zero( input ) ); /* ERROR */
}
