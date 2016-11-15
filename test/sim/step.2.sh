. lib/testcase

sim $TESTS/c/pointer-arith.2.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at $TESTS/c/pointer-arith.c:5
> step --over
+ ^# executing main at $TESTS/c/pointer-arith.c:6
EOF
