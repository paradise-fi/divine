# TAGS: min
. lib/testcase

cp $TESTS/lang-c/ptr-arith.c .

sim ptr-arith.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over
+ ^# executing main at ptr-arith.c:8
> step --over
+ ^# executing main at ptr-arith.c:9
EOF

sim ptr-arith.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step
+ ^# executing main at ptr-arith.c:8
> step
+ ^# executing main at ptr-arith.c:9
EOF

sim ptr-arith.c <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> step --over
+ ^# executing main at ptr-arith.c:8
> step --over
+ ^# executing main at ptr-arith.c:9
EOF
