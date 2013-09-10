. lib

llvm_verify invalid assert memcpy <<EOF
#include <string.h>
void main() {
    char *mem = malloc(4);
    memcpy( mem + 1, mem, 2 );
}
EOF

llvm_verify invalid assert memcpy <<EOF
#include <string.h>
void main() {
    char *mem = malloc(4);
    memcpy( mem, mem + 1, 2 );
}
EOF

llvm_verify valid <<EOF
#include <string.h>
#include <assert.h>
void main() {
    char *mem = malloc(4);
    mem[0] = 4;
    memmove( mem + 1, mem, 2 );
    assert( mem[0] == 4 );
    assert( mem[1] == 4 );
}
EOF
