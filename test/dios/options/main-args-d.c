/* TAGS: min c */
// VERIFY_OPTS: -E a=b -Ec=d -Efootball
// SKIP_CC: 1

#include <dios.h>
#include <assert.h>
#include <string.h>

int main( int argc, char **argv, char **envp )
{
    assert( argc == 1 );
    const char* test_name = "main-args-d.c";
    int tl = strlen( test_name );
    int l = strlen( argv[0] );
    __dios_trace_f( "Binary name: %s", argv[0] + l - tl );
    assert( strcmp( argv[0] + l - tl, test_name ) == 0 );
    assert( argv[1] == NULL);

    assert( envp != NULL );
    assert( strcmp( envp[0], "a=b" ) == 0 );
    assert( strcmp( envp[1], "c=d" ) == 0 );
    assert( strcmp( envp[2], "football") == 0 );
    assert( envp[3] == NULL );
}
