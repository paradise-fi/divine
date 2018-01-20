/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>

int main() {
    int original = umask( 123 );
    assert( umask( original | 01000 ) == 123 );
    assert( umask( original ) == original );
    return 0;
}
