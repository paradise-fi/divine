. lib/testcase

sim $TESTS/pthread/2.mutex-good.c <<EOF
+ ^# executing __boot
> start
+ ^# executing main
> break 2.mutex-good.c:19
> stepa --count 100
+ ^# stopped at breakpoint
> stepa
> backtrace \$state
+ symbol: (__pthread_entry|main)
+ symbol: (__pthread_entry|main)
EOF
