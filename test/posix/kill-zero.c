/* TAGS: c */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

int main( int argc, char *argv[] )
{
    pid_t cpid;
    int ret;

    cpid = fork();

    if ( cpid == 0 ) // child
        while( 1 );
    else
        ret = kill( cpid, 0 );

    assert( ret == 0 );
}
