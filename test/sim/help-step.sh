# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/pointer-arith.c

sim $SRC <<EOF
+ ^# executing __boot
> help
+ start
> start
+ ^# executing main
> step --over
+ ^# executing main at $SRC:8
> step --over
+ ^# executing main at $SRC:9
EOF
