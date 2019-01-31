# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/assert.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> setup --debug libc
> break __dios_fault
> break --list
+ ^ 1: __dios_fault
> stepa --count 500
+ ^# stopped at breakpoint __dios_fault
+ ^# executing __dios_fault
EOF

sim $SRC <<EOF
+ ^# executing __boot
> break $SRC:6
> break --list
+ ^ 1: $SRC:6
> stepa --count 500
+ ^# stopped at breakpoint $SRC:6
+ ^# executing main at $SRC:6
EOF

sim $SRC <<EOF
+ ^# executing __boot
> break assert.c:6
> break --list
+ ^ 1: assert.c:6
> stepa --count 500
+ ^# stopped at breakpoint assert.c:6
+ ^# executing main at $SRC:6
EOF
