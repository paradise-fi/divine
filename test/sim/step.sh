# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/ptr-arith.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:8
> step --over
+ ^# executing main at $SRC:9
EOF

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step
+ ^# executing main at $SRC:8
> step
+ ^# executing main at $SRC:9
EOF

sim $SRC <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:8
> step --over
+ ^# executing main at $SRC:9
EOF
