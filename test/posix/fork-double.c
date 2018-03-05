/* TAGS: fork c */
#include <unistd.h>
#include <assert.h>

void child( int fd )
{
    int buffer;
    ssize_t size = read( fd, &buffer, sizeof( int ) );
    assert( size == 4 );
    assert( buffer == 13317 );
}

void parent( int fd )
{
    int buffer = 13317;
    ssize_t size = write( fd, &buffer, sizeof( int ) );
    assert( size == 4 );
}

int main()
{
    int pipefd[2];
    pipe( pipefd );
    pid_t pid = fork();

    if ( pid != 0 )
    {
        assert ( pid == 2 );

        pid_t pid = fork();
        assert( pid == 3 || pid == 0 );
        if ( pid == 0 )
            parent( pipefd[1] );
    }
    else
        child( pipefd[0] );
}
