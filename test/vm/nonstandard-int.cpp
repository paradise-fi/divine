/* TAGS: min c++ */
// CC_OPTS: -Oz
#include <sys/divm.h>

// Constructor of this struct produces a 48b integer in Clang 3.7-6.0 (with -Oz)

struct Evil
{
    bool a, b, c, d, e, f;
    Evil() :
        a( false ), b( false ), c( false ), d( false ), e( false ), f( false ) {}
};

Evil evilFunc()
{
    Evil evil;
    if ( __vm_choose( 2 ) )
        evil.a = true;
    return evil;
}

int main()
{
    Evil evil = evilFunc();
    return 0;
}
