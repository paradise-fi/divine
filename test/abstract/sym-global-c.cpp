/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <cstdint>

struct Widget {
    int64_t value;
};

Widget w;

int main() {
    int val = __sym_val_i32();
    w.value = val % 10;
    assert( w.value < 10 );
}
