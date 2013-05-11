. lib

llvm_verify invalid "division by zero" testcase.c:3 <<EOF
void main() {
    int a = 4;
    a = a / 0;
}
EOF
