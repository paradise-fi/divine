/* TAGS: sym todo */
/* VERIFY_OPTS: --symbolic */
#include <assert.h>
#include <rst/domains.h>

struct Base
{
    virtual int add( int a, int b ) = 0;
};

struct Derived : Base
{
    int add( int a, int b ) override { return a + b; }
};

int main()
{
    Base *adder = new Derived;

    int a = __sym_val_i32();
    int b = 1;

    if ( a > 0 && a < 100 )
        assert( adder->add( a, b ) > 1 );
    else
        return 0;
}
