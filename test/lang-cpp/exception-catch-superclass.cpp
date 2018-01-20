/* TAGS: c++ */
#include <cassert>
#include <stdexcept>
int main()
{
    int x = 0;
    try { throw std::logic_error( "moo" ); }
    catch( std::exception ) {
        x = 5;
    }
    assert( x == 5 );
    return 0;
}
