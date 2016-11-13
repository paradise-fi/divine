. lib/testcase

sim $TESTS/c/assert.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> break _PDCLIB_assert_dios
> break --list
+ ^ 1: _PDCLIB_assert
> stepa
+ ^# executing _PDCLIB_assert_dios
EOF

sim $TESTS/c/assert.c <<EOF
+ ^# executing __boot
> break $TESTS/c/assert.c:5
> break --list
+ ^ 1: $TESTS/c/assert.c:5
> stepa --count 10
+ ^# executing main at $TESTS/c/assert.c:5
EOF
