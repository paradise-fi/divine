. lib/testcase

cat > file.cpp <<EOF
int foo( volatile int *x, int v ) { return *x - v; }

int main() {
    volatile int x = 4;
    return foo( &x, 4 );
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
> bitcode
+ >>.*alloca
+ call .*foo.*%.*[i32 4 d]
+ ret
EOF
