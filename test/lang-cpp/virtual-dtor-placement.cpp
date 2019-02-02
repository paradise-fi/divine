/* TAGS: min c++ */
#include <cassert>
#include <new>

int global = 0;

struct x { virtual ~x() {} };
struct y : x { virtual ~y() { global = 1; } };

int main()
{
    char mem[ 32 ];
    x *a = new (mem) y();
    a->~x();
    assert( global == 1 );
    return 0;
}
