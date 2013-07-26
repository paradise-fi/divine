. lib

llvm_verify valid <<EOF
int array[2];
void main() {
    array[1] = 3;
    __divine_assert( array[1] == 3 );
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

