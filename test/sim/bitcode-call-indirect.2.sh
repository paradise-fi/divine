. lib/testcase

cat > file.cpp <<EOF
int foo( volatile int *x, int v ) { return *x - v; }
int bar( volatile int *x, int v ) { return *x + v; }

int main() {
    volatile int x = 4;
    int (*fun)( volatile int *, int );
    fun = bar;
    fun( &x, 4 );
    fun = foo;
    return fun( &x, 4 );
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
> bitcode
+ >>\s*label %.*:
+ call .*%.*%.*[i32 4 d]
+ call .*%.*%.*[i32 4 d]
+ ret
EOF
