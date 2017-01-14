// PROGRAM_OPTS: test veryveryveryveryveryveryveryverylongarg --xyz -z -z=42 --a==42
// SKIP_CC: 1

#include <dios.h>
#include <assert.h>
#include <string.h>

int main( int argc, char **argv ) {
    assert( argc == 7 );

    const char* test_name = "2.main-args-a.c";
    int tl = strlen( test_name );
    int l = strlen( argv[0] );
    __dios_trace_f( "Binary name: %s", argv[0] + l - tl );
    assert( strcmp( argv[0] + l - tl, test_name ) == 0 );

    assert( strcmp( argv[1], "test" ) == 0 );
    assert( strcmp( argv[2], "veryveryveryveryveryveryveryverylongarg" ) == 0 );
    assert( strcmp( argv[3], "--xyz" ) == 0 );
    assert( strcmp( argv[4], "-z" ) == 0 );
    assert( strcmp( argv[5], "-z=42" ) == 0 );
    assert( strcmp( argv[6], "--a==42" ) == 0 );
    assert( argv[7] == NULL );
}
