/* TAGS: c */
#include <assert.h>

__attribute__((__annotate__("divine.debugfn")))
void fault( int *x )
{
    *x = 0;
}

int main()
{
    fault( 0 );
    assert( 0 ); /* ERROR */
}
