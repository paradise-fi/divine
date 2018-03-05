/* TAGS: fork c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main()
{
    char buf[ 6 ] = {};
    int fd = open( "file", O_CREAT | O_RDWR, 0644 );
    int dupfd = -1;
    assert( fd >= 0 );

    pid_t pid = fork();

    if ( pid != 0 )
    {
        assert( write( fd, "abcde", 5 ) == 5 );
        assert( dupfd == -1 );
        dupfd = dup( fd );
        assert( dupfd >= 0 );
        assert( dupfd != fd );

        assert( lseek( dupfd, 0, SEEK_SET ) == 0 );
        assert( close( fd ) == 0 );
        assert( read( dupfd, buf, 5 ) == 5 );
        assert( strcmp( buf, "abcde" ) == 0 );

        assert( close( dupfd ) == 0 );
    }
    else
    {
        assert( dupfd == -1 );
        assert( write( fd, "fg", 2 ) == 2 );
        assert( lseek( fd, 0, SEEK_CUR ) == 2 );

        dupfd = dup2( fd, 6 );
        assert( dupfd == 6 );

        assert( lseek( dupfd, 1, SEEK_CUR ) == 3 );
        assert( lseek( fd, 0, SEEK_CUR ) == 3 );

        assert( close( dupfd ) == 0 );
    }

    return 0;
}
