/* TAGS: min c */
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

int main( int argc, char *argv[] )
{
    __dios_configure_fault( _DiOS_F_ExitFault, _DiOS_FC_Ignore );
    pid_t cpid;
    int wstatus;

    cpid = fork();

    if ( cpid == 0 ) // child
        return 1;
    else
    {
        pid_t ret = waitpid( cpid, &wstatus, 0 );

        assert( WIFEXITED( wstatus ) );
        assert( WEXITSTATUS( wstatus ) == 1 );
        assert( !WIFSIGNALED( wstatus ) );
        assert( ret == cpid );
    }
}
