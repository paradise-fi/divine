. lib/testcase

cat > test.c <<EOF
#include <sys/divm.h>

int foo()
{
    __vm_trace( _VM_T_Text, "foo" );
    return 42;
}

int main()
{
    foo();
    foo();
    return 0;
}
EOF

sim test.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main at test.c:11
> step
+ ^# executing foo at test.c:5
EOF

sim test.c <<EOF
> start
> step --count 2
+ ^# executing foo at test.c:6
EOF

sim test.c <<EOF
> start
> step --count 4
+ ^# executing foo at test.c:5
EOF

sim test.c <<EOF
> start
> step --over
+ ^# executing main at test.c:12
EOF

sim test.c <<EOF
> start
> step --over --count 2
+ ^# executing main at test.c:13
EOF
