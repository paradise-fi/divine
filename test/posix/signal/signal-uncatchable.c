/* TAGS: min c */
#include <signal.h>
#include <errno.h>
#include <assert.h>

typedef void (*func)(int);

void f(int i){}

int main()
{
    errno = 0;
    func ret = signal(SIGKILL, f);
    assert( errno == EINVAL );

    raise( SIGKILL );
    assert( false );  //unreachable
    return 0;
}
