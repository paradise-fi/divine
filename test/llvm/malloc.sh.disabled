. lib
. flavour vanilla

llvm_verify invalid "bad dereference" testcase.c:6 <<EOF
#include <stdlib.h>

int main() {
    int *mem = malloc(4);
    if ( mem ) {
        mem[2] = 3;
        free( mem );
    }
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

int main() {
    int *mem = malloc(4);
    if ( mem ) {
        mem[0] = 3;
        free( mem );
    }
    return 0;
}
EOF

llvm_verify invalid "bad dereference" testcase.c:7 <<EOF
#include <stdlib.h>

int main() {
    int *mem = malloc(4);
    if ( mem ) {
        free( mem );
        mem[0] = 3;
    }
    return 0;
}
EOF

llvm_verify invalid "bad argument" . <<EOF
#include <stdlib.h>

int main() {
    int *mem = malloc(65535);
    return 0;
}
EOF

# successfull allocation
llvm_verify invalid "assertion failed" "testcase.c:6" <<EOF
#include <stdlib.h>
#include <assert.h>

int main() {
    int *mem = malloc( sizeof( int ) );
    assert( mem == NULL );
    return 0;
}
EOF

# unsuccessfull allocation
llvm_verify invalid "assertion failed" "testcase.c:6" <<EOF
#include <stdlib.h>
#include <assert.h>

int main() {
    int *mem = malloc( sizeof( int ) );
    assert( mem != NULL );
    return 0;
}
EOF

# calloc should initialize
llvm_verify valid <<EOF
#include <stdlib.h>
#include <assert.h>

int main() {
    int *mem = calloc( 1, sizeof( int ) );
    assert( !mem || *mem == 0 );
    free( mem );
    return 0;
}
EOF
