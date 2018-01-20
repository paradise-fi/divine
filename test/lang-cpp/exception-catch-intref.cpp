/* TAGS: c++ */
#include <cassert>
int main()
{
    try { throw 5; }
    catch( int &x ) {
        assert( x == 5 );
        return 0;
    }
    assert( 0 );
    return 0;
}
