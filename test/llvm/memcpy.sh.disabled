. lib
. flavour vanilla

llvm_verify invalid assert memcpy <<EOF
#include <string.h>
int main() {
    char *mem = malloc(4);
    if ( mem ) {
        memcpy( mem + 1, mem, 2 );
        free( mem );
    }
    return 0;
}
EOF

llvm_verify invalid assert memcpy <<EOF
#include <string.h>
int main() {
    char *mem = malloc(4);
    if ( mem ) {
        memcpy( mem, mem + 1, 2 );
        free( mem );
    }
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <string.h>
#include <assert.h>
int main() {
    char *mem = malloc(4);
    if ( mem ) {
        mem[0] = 4;
        memmove( mem + 1, mem, 2 );
        assert( mem[0] == 4 );
        assert( mem[1] == 4 );
        free( mem );
    }
    return 0;
}
EOF
