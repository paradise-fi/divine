# TAGS: min
. lib/testcase

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> start
- ^# executing
+ ^# executing main at
EOF
