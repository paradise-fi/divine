. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <divine.h>

void thread( void *x ) {}
int main() {
    int tid = __divine_new_thread( thread, 0 );
    if ( tid )
        return 0;
    return 1;
}
EOF

llvm_verify valid <<EOF
union U {
    struct { short a, b; } x;
    int y;
};

int main() {
    union U u;
    u.x.a = 1;
    u.x.b = 2;
    if ( u.y )
        return 0;
    else
        return 1;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:9" <<EOF
union U {
    struct { short a, b; } x;
    int y;
};

int main() {
    union U u;
    u.x.b = 2;
    if ( u.y )
        return 0;
    else
        return 1;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int x;
    if ( x == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int x;
    if ( x )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int x;
    if ( !x )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int x, y;
    if ( x == y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    float x, y;
    if ( x == y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int x, y = 1;
    if ( x == y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    float x, y = 1;
    if ( x == y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int x = 1, y;
    if ( x == y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    float x = 1, y;
    if ( x == y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

int main() {
    int x;
    int y = x == 42;
    if ( y )
        exit( 1 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

int main() {
    int x;
    x = 42;
    if ( x == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

int main() {
    int x;
    int y = x;
    if ( y == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:8" <<EOF
#include <stdlib.h>
#include <string.h>

int main() {
    int x;
    int y = 42;
    memcpy( &y, &x, sizeof( int ) );
    if ( y == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:8" <<EOF
#include <stdlib.h>
#include <string.h>

int main() {
    int x;
    int y = 42;
    memmove( &y, &x, sizeof( int ) );
    if ( y == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

int main() {
    short x = 42;
    int y = x;
    if ( y == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

int main() {
    int x = 42;
    int y = ((short *)&x)[1];
    if ( y == 1 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#define CHECK_DEF( x ) ((void)((x) ? 1 : 2))

union X {
    int i;
    struct {
        short x;
        short y:15;
        short z:1;
    } s;
};

int main() {
    long x = 42;
    CHECK_DEF( x );
    CHECK_DEF( x >> 32 );
    CHECK_DEF( (int)x );
    CHECK_DEF( (short)x );
    CHECK_DEF( (float)x );
    CHECK_DEF( ((short*)&x)[ 2 ] );
    union X u = { .i = x };
    CHECK_DEF( u.i );
    CHECK_DEF( u.s.x );
    CHECK_DEF( u.s.y );
    CHECK_DEF( u.s.z );
    short y[4] = { 42 };
    CHECK_DEF( *y );
    CHECK_DEF( y[1] );
    CHECK_DEF( y[2] );
    CHECK_DEF( y[3] );
    CHECK_DEF( (int)*y );
    CHECK_DEF( (long)*y );
    CHECK_DEF( (float)*y );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

int main() {
    int x;
    int y = x + 4;
    if ( y == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

int main() {
    int x;
    int y = x | 4;
    if ( y == 0 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:10" <<EOF
#include <stdlib.h>

int main() {
    int x;
    int y = x;
    y = 4;
    if ( y == 0 ) // OK
        exit( 1 );
    y |= x;
    if ( y == 0 ) // should fail
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

int main() {
    int *mem = malloc( sizeof( int ) );
    if ( mem && *mem )
        *mem = 42;
    free( mem );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

int main() {
    int *mem = calloc( 1, sizeof( int ) );
    if ( mem && *mem == 0 )
        *mem = 42;
    free( mem );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

int main() {
    int x = 42;
    int *mem = malloc( sizeof( int ) );
    if ( mem ) {
        memcpy( mem, &x, sizeof( int ));
        if ( !*mem )
            exit( 1 );
    }
    free( mem );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:12" <<EOF
#include <stdlib.h>
#include <stdlib.h>

int main() {
    int x;
    int *mem = malloc( sizeof( int ) );
    if ( mem ) {
        *mem = 42;
        if ( !*mem ) // OK
            exit( 1 );
        memcpy( mem, &x, sizeof( int ));
        if ( *mem ) // should fail
            exit( 1 );
    }
    free( mem );
    return 0;
}
EOF

llvm_verify valid <<EOF
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
EOF

llvm_verify_cpp invalid "undefined value" "testcase.cpp:12" <<EOF
#include <cstdlib>

struct Test {
    int x;
    int y;
    Test() : y( 0 ) { }
};

int main() {
    Test x;
    if ( x.y != 0 ) std::exit( 1 ); // OK 
    if ( x.x != 0 ) std::exit( 1 ); // should fail
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:3" << EOF
int main() {
    int x;
    if ( x ? 0 : 42 )
        exit( 1 );
    return 0;
}
EOF

llvm_verify invalid "undefined value" "testcase.c:3" << EOF
int main() {
    int x;
    switch ( x ) { case 0: exit( 2 ); default: exit( 1 ); }
    return 0;
}
EOF

llvm_verify invalid "undefined value" "_PDCLIB_Exit" << EOF
void main() {
}
EOF
