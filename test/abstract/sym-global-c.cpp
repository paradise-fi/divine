/* TAGS: sym min c++ todo */
/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>
#include <cstdint>

struct Widget {
    int64_t value;
};

Widget w;

int main() {
    _SYM int val;
    w.value = val % 10;
    assert( w.value < 10 );
}
