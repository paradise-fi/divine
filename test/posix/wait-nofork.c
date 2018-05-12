/* TAGS: min c */
#include <sys/wait.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>

int main( int argc, char *argv[] )
{
    __dios_configure_fault( _DiOS_F_Exit, _DiOS_FC_Ignore );
    int wstatus;

    pid_t ret = waitpid( 0, &wstatus, -2 );
    assert( ret == -1 );
    assert( errno == EINVAL );

    ret = waitpid( 0, &wstatus, WCONTINUED | WNOHANG );
    assert( ret == -1 );
    assert( errno == ECHILD );
}
