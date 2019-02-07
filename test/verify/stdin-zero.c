/* TAGS: min */
/* VERIFY_OPTS: --stdin `dirname $1`/stdin-zero.bin -o nofail:malloc */
#include <assert.h>
#include <unistd.h>

int main()
{
    char buf[5];
    char want[] = { 0xde, 0xad, 0, 0xbe, 0xef };
    assert( read( 0, &buf, 5 ) == 5 );
    assert( !memcmp( buf, want, 5 ) );
    assert( read( 0, &buf, 1 ) == 0 );
    assert( 0 ); /* ERROR */
}
