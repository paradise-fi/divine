# TAGS:
. lib/testcase

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> setup --sticky "source"
> setup --sticky "backtrace"
> start
+ ^\s*4 int main\(\)
+   main at
+   _start at
EOF

sim $TESTS/lang-c/assert.c <<EOF
+ ^# executing __boot at
> start
> setup --sticky "backtrace $state"
> stepa
EOF
