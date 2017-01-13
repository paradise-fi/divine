. lib/testcase

sim $TESTS/c/1.assert.c <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
EOF
