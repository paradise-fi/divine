. lib/testcase

sim $TESTS/c/malloc.1.c <<EOF
+ ^# executing __boot at
> trace 0 0 1
- FAULT
+ ^traced states: #1 #2
+ ^# executing malloc at
> step --count 3
+ ^# executing - at -
> trace 0 0 0
+ ^traced states: #1 #2
+ ^# executing malloc at
> step --count 5
+ ^T: FAULT: null pointer dereference
+ ^# executing __dios::Fault::handler
> trace 0 0 0 0
+ \[loop closed, unused choices: 0\]
EOF
