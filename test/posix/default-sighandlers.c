/* TAGS: fork c */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

/* Check that signal 2 does not cause a fault but
 * kills the process (an off-by-one check) */

int main( int argc, char *argv[] )
{
    pid_t cpid;
    int ret;

    cpid = fork();

    if ( cpid == 0 ) // child
        while( 1 );
    else
        ret = kill( cpid, 2 );

    assert( ret == 0 );
}
