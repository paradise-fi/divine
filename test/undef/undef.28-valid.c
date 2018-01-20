/* TAGS: min c */
#include <stdlib.h>
#define check( x ) if ( !(x) ) exit( 1 )
struct Test {
    int a;
    int b:8;
    int c:8;
    int d:12;
    int e:4;
};

int main() {
    struct Test x = { .a = 1, .b = 2, .c = 3, .d = 4, .e = 4 };
    check( x.a );
    check( x.b );
    check( x.c );
    check( x.d );
    check( x.e );
    struct Test y = {};
    y.a = 1;
    y.b = 2;
    y.c = 3;
    y.d = 4;
    y.e = 5;
    check( y.a );
    check( y.b );
    check( y.c );
    check( y.d );
    check( y.e );
    return 0;
}
