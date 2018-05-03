# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/ptr-arith.c

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
