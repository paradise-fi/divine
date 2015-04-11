. lib
. flavour vanilla

llvm_verify invalid "bad dereference" testcase.c:10 <<EOF
#include <assert.h>

int *test() {
    int x = 3;
    return &x;
}

int main() {
    int *x = test();
    assert( *x == 3 );
    return 0;
}
EOF
