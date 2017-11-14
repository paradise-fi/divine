#include <stdlib.h>
#include <assert.h>

int main() {
    struct bitfield {
        unsigned flags:5;
    } bf;

    bf.flags = 0x13;
    assert( bf.flags == 0x13 );

    return 0;
}
