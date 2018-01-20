# TAGS: min
. lib/testcase

SRC=$TESTS/lang-c/assert.c

sim $SRC <<EOF
+ ^# executing __boot
> setup --debug-everything
> start
+ ^# executing main
> stepa
+ ^# executing .*fault_handler
> up
> up
> up
+ ^#.*frame in .*run_scheduler
> up
> up
> up
> up
+ ^#.*frame in __dios_fault
> up
+ ^#.*frame in main
> show
+ symbol:.*main
> down
> down
> down
> down
> down
> down
> down
> down
> show
+ symbol:.*fault_handler
EOF
