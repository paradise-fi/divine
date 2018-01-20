/* TAGS: min c */
#include <stdlib.h>
#include <assert.h>

int main() {
    const char *test = getenv( "TEST" );
    assert( !test );
}
