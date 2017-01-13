#include <assert.h>
#include <limits.h>
#include <stdint.h>

int main() {
    float f = 1e20;
    int x = f;
    assert( x ); /* ERROR */
}
