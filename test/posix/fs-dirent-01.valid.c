/* TAGS: c */
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    int fDot = 0;
    int f2Dots = 0;
    int fDir = 0;

    DIR *d = opendir( "." );
    assert( d );

    struct dirent *item;

    while ( (item = readdir( d )) ) {
        if ( strcmp( item->d_name, "." ) == 0 ) {
            assert( !fDot );
            fDot = 1;
        }
        else if ( strcmp( item->d_name, ".." ) == 0 ) {
            assert( !f2Dots );
            f2Dots = 1;
        }
        else if ( strcmp( item->d_name, "dir" ) == 0 ) {
            assert( !fDir );
            fDir = 1;
        }
        else {
            assert( 0 );
        }
    }

    assert( fDot );
    assert( f2Dots );
    assert( fDir );

    assert( closedir( d ) == 0 );

    return 0;
}
