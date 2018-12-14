/* TAGS: c */
#include <signal.h>
#include <errno.h>
#include <assert.h>

typedef void (*func)(int);

void f(int i){}

int main()
{
    errno = 0;
    func ret = signal( 0, f );
    assert( errno == EINVAL );

    assert( raise( 0 ) == 0 );
    return 0;
}
