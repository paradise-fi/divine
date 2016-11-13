. lib/testcase

sim $TESTS/c/pointer-arith.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at $TESTS/c/pointer-arith.c:5
> step --over
+ ^# executing main at $TESTS/c/pointer-arith.c:6
EOF
