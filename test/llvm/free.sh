. lib
. flavour vanilla

llvm_verify invalid "bad argument" '<free>' <<EOF
#include <stdlib.h>

void main() {
    int *mem = malloc(4);
    free( mem );
    free( mem );
}
EOF
