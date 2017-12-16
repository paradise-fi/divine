. lib/testcase

cat > file.cpp <<EOF
void foo( volatile int *x ) { throw 0; }

int main() {
    volatile int x = 0;
    void (*fun)( volatile int * );
    fun = foo;
    try {
        fun( &x );
    } catch ( ... ) { }
    fun( &x );
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
> bitcode
+ >>.*alloca
+ invoke .*%.*label %.*label %
+ label %.*:
+ landingpad
+ call @__cxa_begin_catch
+ call @__cxa_end_catch
+ ret
EOF
