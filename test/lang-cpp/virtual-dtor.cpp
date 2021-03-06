/* TAGS: min c++ */
#include <cassert>
#include <new>

int global = 0;

struct x { virtual ~x() {} };
struct y : x { virtual ~y() { global = 1; } };

int main()
{
    if ( x *a = new (std::nothrow) y() )
    {
        delete a;
        assert( global == 1 );
    }
    return 0;
}
