/* TAGS: c */
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

const char *names[] = {
    ".", "..", "dir"
};

void check( const char *path, int level ) {
    struct dirent **namelist;
    int n = scandir( path, &namelist, NULL, alphasort );
    assert( n == level );

    for ( int i = 0; i < n; ++i ) {
        assert( strcmp( names[ i ], namelist[ i ]->d_name ) == 0 );
        free( namelist[ i ] );
    }
    free( namelist );
}

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    check( "", 3 );
    check( "dir", 2 );
    assert( chdir( "dir" ) == 0 );
    check( "", 2 );

    return 0;
}
