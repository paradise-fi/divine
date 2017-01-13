#include <cassert>
#include <new>

int global = 0;

struct x { virtual ~x() {} };
struct y : x { virtual ~y() { global = 1; } };
struct z : x { virtual ~z() {} };

int main()
{
    char mem[ 32 ];
    x *a = new (mem) y();
    y *b = dynamic_cast< y * >( a );
    z *c = dynamic_cast< z * >( a );
    assert( a == b );
    assert( c == 0 );
    return 0;
}
