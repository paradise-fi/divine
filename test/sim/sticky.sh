# TAGS: min
. lib/testcase

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> setup --sticky "source"
> start
+ ^\s*4 int main\(\)
EOF

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> start
> setup --sticky "backtrace $state"
> stepi
+   main at
+   __dios_start at
EOF
