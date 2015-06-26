. lib
. flavour vanilla

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

void main() {
    int x;
    if ( x == 0 )
        exit( 1 );
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

void main() {
    int x;
    x = 42;
    if ( x == 0 )
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

void main() {
    int x;
    int y = x;
    if ( y == 0 )
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:8" <<EOF
#include <stdlib.h>
#include <string.h>

void main() {
    int x;
    int y = 42;
    memcpy( &y, &x, sizeof( int ) );
    if ( y == 0 )
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:8" <<EOF
#include <stdlib.h>
#include <string.h>

void main() {
    int x;
    int y = 42;
    memmove( &y, &x, sizeof( int ) );
    if ( y == 0 )
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

void main() {
    int x;
    int y = x + 4;
    if ( y == 0 )
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:6" <<EOF
#include <stdlib.h>

void main() {
    int x;
    int y = x | 4;
    if ( y == 0 )
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:10" <<EOF
#include <stdlib.h>

void main() {
    int x;
    int y = x;
    y = 4;
    if ( y == 0 ) // OK
        exit( 1 );
    y |= x;
    if ( y == 0 ) // should fail
        exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:5" <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc( sizeof( int ) );
    if ( mem && *mem )
        *mem = 42;
    free( mem );
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

void main() {
    int x = 42;
    int *mem = malloc( sizeof( int ) );
    if ( mem ) {
        memcpy( mem, &x, sizeof( int ));
        if ( !*mem )
            exit( 1 );
    }
    free( mem );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:12" <<EOF
#include <stdlib.h>
#include <stdlib.h>

void main() {
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

void main() {
    struct Test x = { .a = 1, .b = 2, .c = 3, .d = 4, .e = 4 };
    check( x.a );
    check( x.b );
    check( x.c );
    check( x.d );
    check( x.e );
    struct Test y;
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
    if ( x.y == 0 ) std::exit( 1 ); // OK 
    if ( x.x == 0 ) std::exit( 1 ); // should fail
}
EOF

llvm_verify invalid "undefined value" "testcase.c:3" << EOF
void main() {
    int x;
    if ( x ? 0 : 42 )
        std::exit( 1 );
}
EOF

llvm_verify invalid "undefined value" "testcase.c:3" << EOF
void main() {
    int x;
    switch ( x ) {
        case 0:
            exit( 0 );
        default:
            exit( 1 );
    }
}
EOF
