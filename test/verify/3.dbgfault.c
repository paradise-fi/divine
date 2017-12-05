#include <assert.h>

/* ERRSPEC: fault during dbg.call */

__attribute__((__annotate__("divine.debugfn")))
void fault( int *x )
{
    *x = 0;
}

int main()
{
    fault( 0 );
    assert( 0 );
}
