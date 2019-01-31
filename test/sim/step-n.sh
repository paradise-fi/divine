. lib/testcase

SRC=$TESTS/lang-c/ptr-arith.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over --count 2
+ ^# executing main at $SRC:9
EOF

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --count 2
+ ^# executing main at $SRC:9
EOF

sim $SRC <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> step --over --count 2
+ ^# executing main at $SRC:9
EOF
