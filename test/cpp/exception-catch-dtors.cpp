#include <cassert>
#include <exception>

int a = 0;
int b = 0;

struct destructible
{
    ~destructible() { a = 1; }
};

int throws()
{
    std::exception blergh;
    throw blergh;
}

int dt()
{
    destructible x;
    try {
        throws();
    } catch (...) {
        a = b = 3;
        throw;
    }
}

int main()
{
    int x = 0;
    try {
        x = 3;
        dt();
    } catch (std::exception &e) {
        x = 8;
    } catch (...) {
        x = 2;
    }
    assert( x == 8 );
    assert( a == 1 );
    assert( b == 3 );
    return 0;
}
