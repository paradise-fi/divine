. lib/testcase

SRC=$TESTS/c/1.assert.c

sim $SRC <<EOF
> stepa
> stepa
> rewind #1
! ERROR
+ executing _start
EOF
