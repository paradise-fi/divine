#include <cassert>
#include <new>

struct X { X() { throw 0; } };

int main()
{
    int y = 0;
    try { X x; }
    catch( int ) {
        y = 2;
    }
    assert( y == 2 );
    return 0;
}
