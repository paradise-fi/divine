#include <dios.h>
#include <assert.h>
#include <string.h>

int main( int argc, char **argv ) {
    assert( argc == 1 );

    const char* test_name = "main-args-b.2.c";
    int tl = strlen( test_name );
    int l = strlen( argv[0] );
    __dios_trace_f( "Binary name: %s", argv[0] + l - tl );
    assert( strcmp( argv[0] + l - tl, test_name ) == 0 );
    assert( argv[1] == NULL );
}
