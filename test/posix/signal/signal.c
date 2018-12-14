/* TAGS: min c */
#include <signal.h>
#include <stdbool.h>
#include <assert.h>

bool f_called = false;

typedef void (*func)(int);

void f(int i) { f_called = true; }

int main()
{
    func ret = signal( SIGTERM, f );
    raise( SIGTERM );
    assert( f_called );
    return 0;
}
