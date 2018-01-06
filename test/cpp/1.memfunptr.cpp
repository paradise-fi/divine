#include <cassert>

int global = 0;

struct x
{
    void foo() { global = 7; }
    void bar( int x ) { global = x; }
    virtual void vfoo() { global = 17; }
    virtual void vbar( int x ) { global = 10 + x; }
};

struct y : x
{
    virtual void vfoo() { global = 27; }
    virtual void vbar( int x ) { global = 20 + x; }
};

int main()
{
    void (x::*foo)() = &x::foo;
    void (x::*bar)( int ) = &x::bar;

    x a;
    y b;

    (a.*foo)();
    assert( global == 7 );
    (a.*bar)( 3 );
    assert( global == 3 );

    foo = &x::vfoo;
    bar = &x::vbar;

    (a.*foo)();
    assert( global == 17 );
    (a.*bar)( 3 );
    assert( global == 13 );

    (b.*foo)();
    assert( global == 27 );
    (b.*bar)( 3 );
    assert( global == 23 );
}
