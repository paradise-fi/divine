#include <stdint.h>
#define CHECK_DEF( x ) ((void)((x) ? 1 : 2))

union X {
    int i;
    struct {
        short x;
        short y:15;
        short z:1;
    } s;
};

int main() {
    uint64_t x = 42;
    CHECK_DEF( x );
    CHECK_DEF( x >> 32 );
    CHECK_DEF( (int)x );
    CHECK_DEF( (short)x );
    CHECK_DEF( (float)x );
    CHECK_DEF( ((uint16_t*)&x)[ 2 ] );
    union X u = { .i = x };
    CHECK_DEF( u.i );
    CHECK_DEF( u.s.x );
    CHECK_DEF( u.s.y );
    CHECK_DEF( u.s.z );
    uint16_t y[4] = { 42 };
    CHECK_DEF( *y );
    CHECK_DEF( y[1] );
    CHECK_DEF( y[2] );
    CHECK_DEF( y[3] );
    CHECK_DEF( (uint32_t)*y );
    CHECK_DEF( (uint64_t)*y );
    CHECK_DEF( (float)*y );
    return 0;
}
