/* TAGS: min c */
// VERIFY_OPTS: -Enotpass -E TEST=passed
#include <stdlib.h>
#include <assert.h>

int main() {
    const char *test = getenv( "TEST" );
    assert( test );
    assert( strcmp( test, "passed" ) == 0 );
}
