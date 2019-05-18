# TAGS: min
. lib/testcase

SRC=$TESTS/demo/deadlock.c

sim $SRC <<EOF
> setup --debug libc
> stepa
> stepa
> rewind #1
! ERROR
+ executing __dios_start
EOF
