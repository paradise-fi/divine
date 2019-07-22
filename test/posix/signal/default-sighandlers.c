/* TAGS: fork c */
/* VERIFY_OPTS: --leakcheck none */

/* FIXME It is pretty hard to avoid leaks when a process is killed and DiOS
 * currently fails at it. Hence the --leakcheck none above. */

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
