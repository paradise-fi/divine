/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <errno.h>

void test( int( *toTest )( int ) ) {
    assert( toTest( 0 ) == 0 );
    errno = 0;
    assert( toTest( 20 ) == -1 );
    assert( errno == EBADF );
}

int main() {
    test( fsync );
    test( fdatasync );
    // test( syncfs ); this only works on linux
}
