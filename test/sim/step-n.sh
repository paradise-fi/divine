. lib/testcase

cp $TESTS/lang-c/ptr-arith.c .

sim ptr-arith.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --over --count 2
+ ^# executing main at ptr-arith.c:9
EOF

sim ptr-arith.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> step --count 2
+ ^# executing main at ptr-arith.c:9
EOF

sim ptr-arith.c <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> step --over --count 2
+ ^# executing main at ptr-arith.c:9
EOF
