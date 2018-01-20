/* TAGS: min c */
#include <assert.h>

int main() {
    int x = 1;
    float f = x;
    f += 0.2;
    int y = f;
    assert( x == y );
}
