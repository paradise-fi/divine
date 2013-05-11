. lib

llvm_verify invalid "bad argument" cstdlib <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc(4);
    free( mem );
    free( mem );
}
EOF
