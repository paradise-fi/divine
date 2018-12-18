/* TAGS: todo */
/* VERIFY_OPTS: --symbolic */
#include <assert.h>
#include <rst/domains.h>

int indirect_add( int a, int b )
{
    return a + b;
}

int (*add)() = 0;

int main()
{
    add = indirect_add;

    int a = __sym_val_i32();
    int b = 1;

    if ( a > 0 && a < 100 )
        assert( add( a, b ) > 1 );
    else
        return 0;
}
