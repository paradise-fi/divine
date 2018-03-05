/* TAGS: min c */
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

int main( int argc, char *argv[] )
{
    pid_t cpid;
    int wstatus;

    cpid = fork();

    if ( cpid == 0 ) // child
        while( 1 );
    else
    {
        kill( cpid, SIGINT );
        pid_t ret = waitpid( cpid, &wstatus, 0 );

        assert( WIFSIGNALED( wstatus ) );
        assert( WTERMSIG( wstatus ) == SIGINT );
        assert( !WIFEXITED( wstatus ) );
        assert( ret == cpid );
    }
}
