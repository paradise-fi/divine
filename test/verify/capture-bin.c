/* TAGS: min */
/* VERIFY_OPTS: --capture $SRC_DIR/stdin.bin:nofollow:/file -o nofail:malloc */
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    int fd = open( "/file", O_RDONLY );
    char buf[4];
    char want[] = { 0xde, 0xad, 0xbe, 0xef };
    assert( read( fd, &buf, 4 ) == 4 );
    assert( !memcmp( buf, want, 4 ) );
    assert( read( fd, &buf, 4 ) == 0 );
}
