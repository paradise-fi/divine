. lib/testcase

sim $TESTS/c/1.assert.c <<EOF
+ ^# executing __boot at
> setup --sticky "source"
> setup --sticky "backtrace"
> start
+ ^\s*3 int main\(\)
+ \s*symbol: main
+ \s*symbol: _start
EOF

sim $TESTS/c/1.assert.c <<EOF
+ ^# executing __boot at
> start
> setup --sticky "backtrace $state"
> stepa
EOF
