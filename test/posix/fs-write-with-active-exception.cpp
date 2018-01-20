/* TAGS: c++ */
#include <cstdio>
#include <exception>
#include <unistd.h>
#include <cassert>
#include <errno.h>

struct X {
    ~X() {
        char buf[4] = { 0 };
        int r = write( 42, buf, 4 );
        assert( r == -1 );
        assert( errno == EBADF );
        std::printf( "~X, exception=%d\n", int( std::uncaught_exception() ) );
    }
};

void bar() {
    throw 0;
}

void foo() {
    X x;
    bar();
}

int main() {
    try {
        foo();
    } catch ( ... ) {
        return 0;
    }
    return 1;
}
