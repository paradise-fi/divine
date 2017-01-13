#include <unistd.h>
#include <assert.h>

int main() {
    int original = umask( 123 );
    assert( umask( original | 01000 ) == 123 );
    assert( umask( original ) == original );
    return 0;
}
