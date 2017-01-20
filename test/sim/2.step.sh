. lib/testcase

SRC=$TESTS/c/2.pointer-arith.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:7
> step --over
+ ^# executing main at $SRC:8
EOF
