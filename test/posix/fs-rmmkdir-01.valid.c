/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

char _n1[ 256 ];
char _n2[ 511 ];
char _n3[ 766 ];
char _n4[ 1021 ];
char _n5[ 1030 ];

char *name[] = {
    _n1, _n2, _n3, _n4, _n5
};

int main() {

    memset( name[ 0 ], 'a', 255 );
    memset( name[ 1 ], 'a', 510 );
    memset( name[ 2 ], 'a', 765 );
    memset( name[ 3 ], 'a', 1020 );

    name[ 0 ][ 255 ] = '\0';
    name[ 1 ][ 510 ] = '\0';
    name[ 2 ][ 765 ] = '\0';
    name[ 3 ][ 1020 ] = '\0';

    name[ 1 ][ 255 ] = '/';
    name[ 2 ][ 255 ] = '/';
    name[ 2 ][ 510 ] = '/';
    name[ 3 ][ 255 ] = '/';
    name[ 3 ][ 510 ] = '/';
    name[ 3 ][ 765 ] = '/';

    memcpy( name[ 4 ], name[ 3 ], 1021 );
    strcat( name[ 4 ] + 1020, "/aaaaaaa" );

    assert( mkdir( name[0], 0755 ) == 0 );
    errno = 0;
    assert( mkdir( name[0], 0755 ) == -1 );
    assert( errno == EEXIST );

    name[ 1 ][ 255 ] = 'a';
    errno = 0;
    assert( mkdir( name[ 1 ], 0755 ) == -1 );
    assert( errno == ENAMETOOLONG );
    name[ 1 ][ 255 ] = '/';

    for ( int i = 1; i < 4; ++i )
        assert( mkdir( name[ i ], 0755 ) == 0 );

    assert( access( name[ 3 ], F_OK ) == 0 );
    errno = 0;
    assert( mkdir( name[ 4 ], 0755 ) == -1 );
    assert( errno == ENAMETOOLONG );
    return 0;
}
