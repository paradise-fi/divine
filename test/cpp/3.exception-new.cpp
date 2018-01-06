#include <cassert>
#include <new>

int main()
{
    int *x = nullptr;
    int y = 0;
    try { x = new int; y = 1; }
    catch( std::exception ) {
        y = 2;
    }
    assert( ( x && y == 1 ) || ( !x && y == 2 ) );
    return 0;
}
