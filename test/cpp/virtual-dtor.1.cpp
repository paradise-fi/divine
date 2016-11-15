#include <cassert>
#include <new>

int global = 0;

struct x { virtual ~x() {} };
struct y : x { virtual ~y() { global = 1; } };

int main()
{
    char mem[ 32 ];
    x *a = new (mem) y();
    delete a;
    assert( global == 1 );
    return 0;
}
