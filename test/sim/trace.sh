# TAGS: min
. lib/testcase

sim $TESTS/lang-c/malloc.c <<EOF
+ ^# executing __boot at
> setup --debug libc
> trace --choices 0 1
- FAULT
+ ^traced states: #1
+ ^# executing malloc at
> stepa --count 3
+ ^# executing - at -
> trace --choices 0 0
+ ^traced states: #1
+ ^# executing malloc at
> step --out
> step --count 2
+ ^T: FAULT: null pointer dereference
+ ^# executing.*{Fault}::handler
> trace --choices 0 0 0
+ traced states: #1 #3
+ unused choices: 0/0
EOF
