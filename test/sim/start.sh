. lib/testcase

sim $TESTS/c/assert.c <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
EOF
