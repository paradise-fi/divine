/* TAGS: min */
/* VERIFY_OPTS: --stdin $SRC_DIR/stdin.bin -o nofail:malloc */
#include <assert.h>
#include <unistd.h>

int main()
{
    char buf[4];
    char want[] = { 0xde, 0xad, 0xbe, 0xef };
    assert( read( 0, &buf, 4 ) == 4 );
    assert( !memcmp( buf, want, 4 ) );
    assert( read( 0, &buf, 4 ) == 0 );
}
