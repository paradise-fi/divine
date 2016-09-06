. lib
. flavour vanilla

llvm_verify invalid "bad argument" '<free>' <<EOF
#include <stdlib.h>

int main() {
    int *mem = malloc(4);
    free( mem );
    free( mem );
    return 0;
}
EOF
