. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <assert.h>
int array[2];
void main() {
    array[1] = 3;
    assert( array[1] == 3 );
}
EOF

llvm_verify invalid "bad dereference" testcase.c:3 <<EOF
int array[2];
void main() {
    array[2] = 3;
}
EOF

llvm_verify valid <<EOF
int array[2];
void main() {
    memset( array, 1, 2 * sizeof( int ) );
}
EOF

llvm_verify invalid "bad dereference" memset.c <<EOF
int array[2];
void main() {
    memset( array, 1, 3 * sizeof( int ) );
}
EOF

llvm_verify valid <<EOF
struct self { struct self *a; };
struct self s = { &s };
void main() {}
EOF

llvm_verify valid <<EOF
#include <assert.h>
int array[2] = { 3, 5 };
void main() {
     assert( array[0] == 3 );
     array[0] = 2;
     assert( array[1] == 5 );
     assert( array[0] == 2 );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
int a[] = {105,106,107,108,109,110};
int (*b)[6] = &a;
int main() {
    int (*c)[6] = &a;
    assert( (*b)[0] == 105 );
    assert( (*c)[0] == 105 );
    assert( (*b)[1] == 106 );
    assert( (*c)[1] == 106 );

    (*b)[0] = 3;
    (*c)[1] = 4;
    a[2] = 1;

    assert( a[0] == 3 );
    assert( (*b)[0] == 3 );
    assert( (*c)[0] == 3 );
    assert( a[1] == 4 );
    assert( (*b)[1] == 4 );
    assert( (*c)[1] == 4 );
    assert( a[2] == 1 );
    assert( (*b)[2] == 1 );
    assert( (*c)[2] == 1 );
    return 0;
}
EOF
