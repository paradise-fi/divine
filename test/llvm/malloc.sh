. lib
. flavour vanilla

llvm_verify invalid "bad dereference" testcase.c:6 <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc(4);
    if ( mem ) {
        mem[2] = 3;
        free( mem );
    }
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc(4);
    if ( mem ) {
        mem[0] = 3;
        free( mem );
    }
}
EOF

llvm_verify invalid "bad dereference" testcase.c:7 <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc(4);
    if ( mem ) {
        free( mem );
        mem[0] = 3;
    }
}
EOF

llvm_verify invalid "bad argument" . <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc(65535);
}
EOF

# successfull allocation
llvm_verify invalid "assertion failed" "testcase.c:6" <<EOF
#include <stdlib.h>
#include <assert.h>

void main() {
    int *mem = malloc( sizeof( int ) );
    assert( mem == NULL );
}
EOF

# unsuccessfull allocation
llvm_verify invalid "assertion failed" "testcase.c:6" <<EOF
#include <stdlib.h>
#include <assert.h>

void main() {
    int *mem = malloc( sizeof( int ) );
    assert( mem != NULL );
}
EOF

# calloc should initialize
llvm_verify valid <<EOF
#include <stdlib.h>
#include <assert.h>

void main() {
    int *mem = calloc( 1, sizeof( int ) );
    assert( mem && *mem == 0 );
    free( mem );
}
EOF
