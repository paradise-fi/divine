/* TAGS: c++ */
#include <cassert>
#include <thread>

/* EXPECT: --result error --symbol __terminate */

void foo() {
    throw 4;
}

int main()
{
    auto die = []
    {
        try
        {
            foo();
        } catch ( long )
        {
            assert( 0 );
        }
        assert( 0 );
    };
    std::thread t( die );
    t.join();
}
