#include <divine.h>
#include <cstdlib>

extern "C" {
    int main(...);
    struct global_ctor { int prio; void (*fn)(); void *data; };

    void _divine_start( int ctorcount, void *ctors, int mainproto )
    {
        int r;
        global_ctor *c = reinterpret_cast< global_ctor * >( ctors );
        for ( int i = 0; i < ctorcount; ++i ) {
            c->fn();
            c ++;
        }
        switch (mainproto) {
            case 0: r = main(); break;
            case 1: r = main( 0 ); break;
            case 2: r = main( 0, 0 ); break;
            case 3: r = main( 0, 0, 0 ); break;
            default: __divine_problem( 1, "don't know how to run main()" );
        }
        exit( r );
    }
}
