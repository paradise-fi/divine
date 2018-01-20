/* TAGS: min c */
#include <assert.h>

struct self { struct self *a; };
struct self s = { &s };

int main()
{
    assert( s.a == &s );
    return 0;
}
