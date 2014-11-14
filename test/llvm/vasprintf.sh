. lib

# OOM
llvm_verify invalid "ASSERTION FAILED" "testcase.c:8" <<EOF
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main( void ) {
    char *buf = NULL;
    int r = asprintf( &buf, "test" );
    assert( r == 4 ); // OOM
    if ( r ) free( buf );
    return 0;
}
EOF

# OOM
llvm_verify invalid "ASSERTION FAILED" "testcase.c:19" <<EOF
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int call_vasprintf( char **out, const char *fmt, ... ) {
    int rc;
    va_list ap;
    va_start( ap, fmt );
    return vasprintf( out, fmt, ap );
    va_end( ap );
    return rc;
}

int main( void ) {
    char *buf = NULL;
    int r = call_vasprintf( &buf, "test" );
    assert( r == 4 ); // OOM
    if ( r ) free( buf );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main( void ) {
    char *buf = NULL;
    int r = asprintf( &buf, "test" );
    if ( r != 0 ) { // !oom
        assert( r == 4 );
        assert( buf != NULL );
        assert( strncmp( "test", buf, 4 ) == 0 );
        free( buf );
    }

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int call_vasprintf( char **out, const char *fmt, ... ) {
    int rc;
    va_list ap;
    va_start( ap, fmt );
    return vasprintf( out, fmt, ap );
    va_end( ap );
    return rc;
}

int main( void ) {
    char *buf = NULL;
    int r = call_vasprintf( &buf, "test" );
    if ( r != 0 ) { // !oom
        assert( r == 4 );
        assert( buf != NULL );
        assert( strncmp( "test", buf, 4 ) == 0 );
        free( buf );
    }

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main( void ) {
    char *buf = NULL;
    int r = asprintf( &buf, "test %d %03ld", 0, 42l );
    if ( r != 0 ) { // !oom
        assert( buf != NULL );
        assert( strcmp( "test 0 042", buf ) == 0 );
        free( buf );
    }

    return 0;
}
EOF
