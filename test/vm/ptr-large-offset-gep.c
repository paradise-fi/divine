/* TAGS: min c */
#include <assert.h>
#include <stdint.h>

int main() {
    char array[20];
    array[0] = 'a';
    array[10] = 'b';

    assert( *(array + 10) == 'b' );
    assert( *(array + ((uint64_t)( 1 ) << 33)) == 'a' ); /* ERROR */
    assert( *(array + ((uint64_t)( 1 ) << 33) + 10) == 'b' );
}
