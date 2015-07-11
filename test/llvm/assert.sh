. lib
. flavour vanilla

llvm_verify invalid "assertion failed" testcase.c:4 <<EOF
#include <assert.h>

int main() {
    assert( 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>

int main() {
    assert( 1 );
    return 0;
}
EOF
