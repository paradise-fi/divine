. lib/testcase

SRC=$TESTS/c/1.assert.c

sim $SRC <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> stepa
+ ^# executing .*fault_handler
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
> show
+ symbol:.*fault_handler
EOF
