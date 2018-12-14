/* TAGS: min c */
#include <signal.h>
#include <errno.h>
#include <assert.h>

typedef void (*func)(int);

void f(int i){}

int main()
{
    struct sigaction sa1, sa2;
    sa2.sa_handler = f;
    int ret = sigaction( SIGKILL, &sa1, &sa2 );
    assert( ret == -1 );
    assert( errno == EINVAL );

    raise( SIGKILL );
    assert( false );  //unreachable
    return 0;
}
