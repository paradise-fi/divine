. lib/testcase

sim $TESTS/c/assert.1.c <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
EOF
