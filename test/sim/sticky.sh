# TAGS: min
. lib/testcase

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> setup --sticky "source"
> start
+ ^\s*3 int main\(\)
EOF

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> start
> setup --sticky "backtrace $state"
> stepi
+   main at
+   _start at
EOF
