/* TAGS: min c */
#include <stdio.h>
#include <assert.h>

int main() {
    char buf[32];

    sprintf( buf + 30, "%d", 0 );
    assert( buf[30] == '0' );
    assert( buf[31] == 0 );

    sprintf( buf + 30, "%d", 1 );
    assert( buf[30] == '1' );
    assert( buf[31] == 0 );

    sprintf( buf + 30, "%d", 4 );
    assert( buf[30] == '4' );
    assert( buf[31] == 0 );

    sprintf( buf + 30, "%d", 9 );
    assert( buf[30] == '9' );
    assert( buf[31] == 0 );

    sprintf( buf + 29, "%d", 10 );
    assert( buf[29] == '1' );
    assert( buf[30] == '0' );
    assert( buf[31] == 0 );

    sprintf( buf + 29, "%d", 42 );
    assert( buf[29] == '4' );
    assert( buf[30] == '2' );
    assert( buf[31] == 0 );

    sprintf( buf + 29, "%d", 99 );
    assert( buf[29] == '9' );
    assert( buf[30] == '9' );
    assert( buf[31] == 0 );

    sprintf( buf + 22, "%d", 123456789 );
    assert( buf[22] == '1' );
    assert( buf[23] == '2' );
    assert( buf[24] == '3' );
    assert( buf[25] == '4' );
    assert( buf[26] == '5' );
    assert( buf[27] == '6' );
    assert( buf[28] == '7' );
    assert( buf[29] == '8' );
    assert( buf[30] == '9' );
    assert( buf[31] == 0 );
}
