. lib

llvm_verify valid <<EOF
#include <stdlib.h>

void main() {
    exit( 0 );
    assert( 0 );
}
EOF
