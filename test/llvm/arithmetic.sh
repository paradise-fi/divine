. lib
. flavour vanilla

llvm_verify invalid "division by zero" testcase.c:3 arithmetic <<EOF
void main() {
    int a = 4;
    a = a / 0;
}
EOF

llvm_verify valid arithmetic <<EOF
#include <assert.h>
void main() {
    int a = -1;
    assert( a < 0 );
}
EOF

llvm_verify valid arithmetic <<EOF
#include <assert.h>
void main() {
    char *p = malloc(4);
    assert( (p + 1) - p == 1 );
    assert( p - (p + 1) == -1 );
    free(p);
}
EOF

