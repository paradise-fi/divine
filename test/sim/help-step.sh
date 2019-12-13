# TAGS: min
. lib/testcase

cp $TESTS/lang-c/ptr-arith.c .

sim ptr-arith.c <<EOF
+ ^# executing __boot
> help
+ start
> start
+ ^# executing main
> step --over
+ ^# executing main at ptr-arith.c:8
> step --over
+ ^# executing main at ptr-arith.c:9
EOF
