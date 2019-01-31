# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/assert.c

sim $SRC <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> stepa --count 100
+ ^# executing .*fault_handler
> up
> up
+ ^#.*frame in __dios_fault
> up
> up
+ ^#.*frame in main
> show
+ symbol:.*main
> down
> down
> down
> down
> show
+ symbol:.*fault_handler
EOF
