/* TAGS: fork min c */
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

int main( int argc, char *argv[] )
{
    __dios_configure_fault( _DiOS_F_Exit, _DiOS_FC_Ignore );
    pid_t cpid;
    int wstatus;

    cpid = fork();

    if ( cpid == 0 ) // child
        while( 1 );
    else
    {
        pid_t ret = waitpid( 0, &wstatus, WNOHANG );
        assert( ret == 0 );
    }
}
