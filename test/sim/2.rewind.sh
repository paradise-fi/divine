. lib/testcase

SRC=$TESTS/demo/1.deadlock.c

sim $SRC <<EOF
> setup --debug libc
> stepa
> stepa
> rewind #1
! ERROR
+ executing _start
EOF
