. lib/testcase

sim $TESTS/pthread/2.mutex-good.c <<EOF
+ ^# executing __boot
> setup --debug libc
> start
+ ^# executing main
> break 2.mutex-good.c:19
> stepa --count 100
+ ^# stopped at breakpoint
> stepa
> backtrace \$state
+   (__pthread_entry|main) at
+   (__pthread_entry|main) at
EOF
