/* TAGS: min c++ */
#include <typeinfo>
#include <cassert>

struct x { virtual ~x() {} };

int main()
{
    auto ti = &typeid( x );
    void **vtable = *(void***)ti;
    assert( vtable[-1] );
    return 0;
}
