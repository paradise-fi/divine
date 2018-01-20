# TAGS:
. lib/testcase

sim $TESTS/pthread/mutex-good.c <<EOF
+ ^# executing __boot
> setup --debug libc
> start
+ ^# executing main
> break mutex-good.c:19
> stepa --count 100
+ ^# stopped at breakpoint
> stepa
> backtrace \$state
+   (__pthread_entry|main) at
+   (__pthread_entry|main) at
EOF
