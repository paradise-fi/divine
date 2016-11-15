#include <cassert>
int main()
{
    try { throw 5; }
    catch( ... ) {}
    assert( 0 ); /* ERROR */
    return 0;
}
