. lib/testcase

SRC=$TESTS/c/pointer-arith.2.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:5
> step --over
+ ^# executing main at $SRC:6
EOF
