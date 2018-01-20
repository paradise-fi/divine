# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/pointer-arith.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:7
> step --over
+ ^# executing main at $SRC:8
EOF

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step
+ ^# executing main at $SRC:7
> step
+ ^# executing main at $SRC:8
EOF

sim $SRC <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:7
> step --over
+ ^# executing main at $SRC:8
EOF
