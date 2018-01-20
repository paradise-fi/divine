/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

void writeFile( const char *name, const char *content ) {
    int fd = open( name, O_CREAT | O_RDWR, 0644 );
    assert( fd >= 0 );

    int size = strlen( content );
    assert( write( fd, content, size ) == size );
    assert( close( fd ) == 0 );
}

int main() {
    char buf[ 8 ] = {};
    writeFile( "file1", "tralala" );
    writeFile( "file2", "hrabala" );

    int fd = open( "file1", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    int newfd = open( "file2", O_RDONLY );
    assert( newfd >= 0 );

    int dupfd = dup2( newfd, fd );
    assert( dupfd == fd );

    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "hrabala" ) == 0 );

    assert( close( fd ) == 0 );
    assert( close( newfd ) == 0 );

    return 0;
}
